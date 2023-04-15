#include "var_lifetime_opt.hpp"

namespace armajitto::ir {

VarLifetimeOptimizerPass::VarLifetimeOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc)
    : OptimizerPassBase(emitter)
    , m_ops(&alloc)
    , m_varAccesses(&alloc)
    , m_rootNodes(&alloc)
    , m_leafNodes(&alloc)
    , m_fwdDeps(&alloc)
    , m_revDeps(&alloc)
    , m_sortedLeafNodes(&alloc)
    , m_maxDistToLeaves(&alloc)
    , m_maxDistFromRoot(&alloc)
    , m_writtenNodes(&alloc) {}

void VarLifetimeOptimizerPass::Reset() {
    AccessRecord emptyRecord{};

    const uint32_t varCount = m_emitter.VariableCount();
    m_varAccesses.resize(varCount);

    const size_t opCount = m_emitter.IROpCount();
    m_ops.resize(opCount);
    m_rootNodes.resize((opCount + 63) / 64);
    m_leafNodes.resize((opCount + 63) / 64);
    m_fwdDeps.resize(opCount);
    m_revDeps.resize(opCount);
    m_maxDistToLeaves.resize(opCount);
    m_maxDistFromRoot.resize(opCount);
    m_writtenNodes.resize((opCount + 63) / 64);

    m_opIndex = 0;

    {
        size_t opIndex = 0;
        IROp *op = m_emitter.GetBlock().Head();
        while (op != nullptr) {
            m_ops[opIndex++] = op;
            op = op->Next();
        }
    }

    std::fill(m_varAccesses.begin(), m_varAccesses.end(), emptyRecord);
    m_gprAccesses.fill(emptyRecord);
    m_psrAccesses.fill(emptyRecord);
    m_flagNAccesses = emptyRecord;
    m_flagZAccesses = emptyRecord;
    m_flagCAccesses = emptyRecord;
    m_flagVAccesses = emptyRecord;

    std::fill(m_rootNodes.begin(), m_rootNodes.end(), ~0ull);
    std::fill(m_leafNodes.begin(), m_leafNodes.end(), ~0ull);
    std::fill(m_maxDistToLeaves.begin(), m_maxDistToLeaves.end(), ~0);
    std::fill(m_maxDistFromRoot.begin(), m_maxDistFromRoot.end(), 0);
    std::fill(m_writtenNodes.begin(), m_writtenNodes.end(), 0);
    for (auto &deps : m_fwdDeps) {
        deps.clear();
    }
    for (auto &deps : m_revDeps) {
        deps.clear();
    }
}

void VarLifetimeOptimizerPass::PostProcess(IROp *op) {
    ++m_opIndex;
}

void VarLifetimeOptimizerPass::PostProcess() {
    /*if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466)*/ {
        const size_t opCount = m_maxDistToLeaves.size();

        printf("------------------------------------------------------------\n");
        auto locStr = m_emitter.GetBlock().Location().ToString();
        printf("block %s, %u instructions:\n", locStr.c_str(), m_emitter.GetBlock().InstructionCount());
        IROp *op = m_emitter.GetBlock().Head();
        size_t opIndex = 0;
        while (op != nullptr) {
            auto opStr = op->ToString();
            printf(" %3zu  %s\n", opIndex, opStr.c_str());
            op = op->Next();
            ++opIndex;
        }
        printf("\n");

        printf("root nodes:");
        for (size_t i = 0; i < m_rootNodes.size(); i++) {
            uint64_t bitmap = m_rootNodes[i];
            size_t bmindex = std::countr_zero(bitmap);
            while (bmindex < 64) {
                const size_t rootIndex = bmindex + i * 64;
                if (rootIndex >= opCount) {
                    break;
                }

                printf(" %zu", rootIndex);

                bitmap &= ~(1ull << bmindex);
                bmindex = std::countr_zero(bitmap);
            }
        }
        printf("\n");

        printf("leaf nodes:");
        for (size_t i = 0; i < m_leafNodes.size(); i++) {
            uint64_t bitmap = m_leafNodes[i];
            size_t bmindex = std::countr_zero(bitmap);
            while (bmindex < 64) {
                const size_t leafIndex = bmindex + i * 64;
                if (leafIndex >= opCount) {
                    break;
                }

                printf(" %zu", leafIndex);

                bitmap &= ~(1ull << bmindex);
                bmindex = std::countr_zero(bitmap);
            }
        }
        printf("\n");

        printf("forward dependencies:\n");
        for (size_t i = 0; i < m_fwdDeps.size(); i++) {
            if (!m_fwdDeps[i].empty()) {
                printf("%zu:", i);
                for (auto dep : m_fwdDeps[i]) {
                    printf(" %zu", dep);
                }
                printf("\n");
            }
        }
        printf("\n");

        printf("reverse dependencies:\n");
        for (size_t i = 0; i < m_revDeps.size(); i++) {
            if (!m_revDeps[i].empty()) {
                printf("%zu:", i);
                for (auto dep : m_revDeps[i]) {
                    printf(" %zu", dep);
                }
                printf("\n");
            }
        }
        printf("\n");
    }

    // Iterate over all nodes in reverse order to compute maximum distances from node to leaves
    const size_t opCount = m_maxDistToLeaves.size();
    for (size_t i = 0; i < opCount; i++) {
        CalcMaxDistanceToLeaves(opCount - i - 1);
    }

    // Iterate over all root nodes to compute maximum distances from node to root
    for (size_t i = 0; i < m_rootNodes.size(); i++) {
        uint64_t bitmap = m_rootNodes[i];
        size_t bmindex = std::countr_zero(bitmap);
        while (bmindex < 64) {
            const size_t rootIndex = bmindex + i * 64;
            if (rootIndex >= opCount) {
                break;
            }

            // Traverse all children nodes, computing maximum distances from root
            for (auto node : m_fwdDeps[rootIndex]) {
                CalcMaxDistanceFromRoot(node);
            }

            bitmap &= ~(1ull << bmindex);
            bmindex = std::countr_zero(bitmap);
        }
    }

    /*if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466)*/ {
        printf("max distances:\n");
        for (size_t i = 0; i < m_maxDistToLeaves.size(); i++) {
            printf("  %zu = %zu // %zu", i, m_maxDistToLeaves[i], m_maxDistFromRoot[i]);
            if (IsRootNode(i)) {
                printf(" (root)");
            }
            if (IsLeafNode(i)) {
                printf(" (leaf)");
            }
            printf("\n");
        }
        printf("\n");
    }

    // Sort leaf nodes from longest to shortest, then highest to lowest index for stability over multiple executions
    m_sortedLeafNodes.clear();
    for (size_t i = 0; i < m_leafNodes.size(); i++) {
        uint64_t bitmap = m_leafNodes[i];
        size_t bmindex = std::countr_zero(bitmap);
        while (bmindex < 64) {
            const size_t leafIndex = bmindex + i * 64;
            if (leafIndex >= opCount) {
                break;
            }
            m_sortedLeafNodes.push_back(leafIndex);
            bitmap &= ~(1ull << bmindex);
            bmindex = std::countr_zero(bitmap);
        }
    }
    std::sort(m_sortedLeafNodes.begin(), m_sortedLeafNodes.end(), [&](uint64_t lhs, uint64_t rhs) {
        if (m_maxDistFromRoot[lhs] > m_maxDistFromRoot[rhs]) {
            return true;
        }
        if (m_maxDistFromRoot[lhs] < m_maxDistFromRoot[rhs]) {
            return false;
        }

        return lhs > rhs;
    });

    /*if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466)*/ {
        printf("sorted leaf nodes:");
        for (auto index : m_sortedLeafNodes) {
            printf(" %zu", index);
        }
        printf("\n");
    }

    // Rewrite instructions
    for (auto leafIndex : m_sortedLeafNodes) {
        Rewrite(leafIndex);
    }

    // Check for changes and mark as dirty if the instruction order changed
    auto *op = m_emitter.GetBlock().Head();
    size_t opIndex = 0;
    while (op != nullptr) {
        if (op != m_ops[opIndex]) {
            MarkDirty();
            break;
        }
        opIndex++;
        op = op->Next();
    }
}

void VarLifetimeOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead();
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    RecordCPSRWrite();
}

void VarLifetimeOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    RecordSPSRWrite(op->mode);
}

void VarLifetimeOptimizerPass::Process(IRMemReadOp *op) {
    RecordRead(op->address);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->address);
    RecordRead(op->src);
}

void VarLifetimeOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    RecordRead(op->value);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitClearOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRAddOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddCarryOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dstLo);
    RecordWrite(op->dstHi);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddLongOp *op) {
    RecordRead(op->lhsLo);
    RecordRead(op->lhsHi);
    RecordRead(op->rhsLo);
    RecordRead(op->rhsHi);
    RecordWrite(op->dstLo);
    RecordWrite(op->dstHi);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordRead(op->values);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordRead(op->srcCPSR);
    RecordRead(op->flags);
    RecordWrite(op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    RecordRead(op->srcCPSR);
    if (op->setQ) {
        RecordRead(arm::Flags::V);
    }
    RecordWrite(op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
    RecordCPSRRead();
    RecordWrite(arm::GPR::PC);
}

void VarLifetimeOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
    RecordCPSRRead();
    RecordWrite(arm::GPR::PC);
    RecordCPSRWrite();
}

void VarLifetimeOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    RecordWrite(op->dstValue);
}

void VarLifetimeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void VarLifetimeOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRCopyVarOp *op) {
    RecordRead(op->var);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst);
}

// ---------------------------------------------------------------------------------------------------------------------
// Read/write tracking

static inline size_t SPSRIndex(arm::Mode mode) {
    return arm::NormalizedIndex(mode) + 1;
}

void VarLifetimeOptimizerPass::ResizeVarAccesses(size_t index) {
    if (m_varAccesses.size() <= index) {
        m_varAccesses.resize(index + 1);
    }
}

void VarLifetimeOptimizerPass::RecordRead(VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordRead(arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordRead(VariableArg arg) {
    if (!arg.var.IsPresent()) {
        return;
    }

    const auto varIndex = arg.var.Index();
    ResizeVarAccesses(varIndex);
    AddReadDependencyEdge(m_varAccesses[varIndex]);
}

void VarLifetimeOptimizerPass::RecordRead(GPRArg arg) {
    AddReadDependencyEdge(m_gprAccesses[arg.Index()]);
}

void VarLifetimeOptimizerPass::RecordCPSRRead() {
    RecordPSRRead(0);
}

void VarLifetimeOptimizerPass::RecordSPSRRead(arm::Mode mode) {
    RecordPSRRead(SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRRead(size_t index) {
    AddReadDependencyEdge(m_psrAccesses[index]);
}

void VarLifetimeOptimizerPass::RecordRead(arm::Flags flags) {
    const auto bmFlags = BitmaskEnum(flags);
    auto update = [&](arm::Flags flag, AccessRecord &accessRecord) {
        if (bmFlags.AnyOf(flag)) {
            AddReadDependencyEdge(accessRecord);
        }
    };
    update(arm::Flags::N, m_flagNAccesses);
    update(arm::Flags::Z, m_flagZAccesses);
    update(arm::Flags::C, m_flagCAccesses);
    update(arm::Flags::V, m_flagVAccesses);
}

void VarLifetimeOptimizerPass::RecordWrite(VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordWrite(arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordWrite(VariableArg arg) {
    if (!arg.var.IsPresent()) {
        return;
    }

    const auto varIndex = arg.var.Index();
    ResizeVarAccesses(varIndex);
    AddWriteDependencyEdge(m_varAccesses[varIndex]);
}

void VarLifetimeOptimizerPass::RecordWrite(GPRArg arg) {
    AddWriteDependencyEdge(m_gprAccesses[arg.Index()]);
}

void VarLifetimeOptimizerPass::RecordCPSRWrite() {
    RecordPSRWrite(0);
}

void VarLifetimeOptimizerPass::RecordSPSRWrite(arm::Mode mode) {
    RecordPSRWrite(SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRWrite(size_t index) {
    AddWriteDependencyEdge(m_psrAccesses[index]);
}

void VarLifetimeOptimizerPass::RecordWrite(arm::Flags flags) {
    const auto bmFlags = BitmaskEnum(flags);
    auto update = [&](arm::Flags flag, AccessRecord &accessRecord) {
        if (bmFlags.AnyOf(flag)) {
            AddWriteDependencyEdge(accessRecord);
        }
    };
    update(arm::Flags::N, m_flagNAccesses);
    update(arm::Flags::Z, m_flagZAccesses);
    update(arm::Flags::C, m_flagCAccesses);
    update(arm::Flags::V, m_flagVAccesses);
}

// ---------------------------------------------------------------------------------------------------------------------
// Dependency graph

void VarLifetimeOptimizerPass::AddReadDependencyEdge(AccessRecord &record) {
    if (record.writeIndex != ~0) {
        AddEdge(record.writeIndex, m_opIndex);
    }
    record.readIndex = m_opIndex;
}

void VarLifetimeOptimizerPass::AddWriteDependencyEdge(AccessRecord &record) {
    if (record.readIndex != ~0) {
        AddEdge(record.readIndex, m_opIndex);
    }
    if (record.writeIndex != ~0) {
        AddEdge(record.writeIndex, m_opIndex);
    }
    record.writeIndex = m_opIndex;
}

void VarLifetimeOptimizerPass::AddEdge(size_t from, size_t to) {
    // Don't add self-dependencies
    if (from == to) {
        return;
    }

    // Don't repeat the same edge
    auto &fwdDeps = m_fwdDeps[from];
    if (!fwdDeps.empty() && fwdDeps.back() == to) {
        return;
    }

    // Add forward edge to graph
    fwdDeps.push_back(to);

    // Add reverse edge to graph
    auto &revDeps = m_revDeps[to];
    auto it = std::lower_bound(revDeps.begin(), revDeps.end(), from);
    revDeps.insert(it, from);

    // Mark "from node as non-leaf
    ClearLeafNode(from);

    // Mark "to" node as non-root
    ClearRootNode(to);
}

bool VarLifetimeOptimizerPass::IsRootNode(size_t index) const {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    return m_rootNodes[vecIndex] & (1ull << bitIndex);
}

void VarLifetimeOptimizerPass::ClearRootNode(size_t index) {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    m_rootNodes[vecIndex] &= ~(1ull << bitIndex);
}

bool VarLifetimeOptimizerPass::IsLeafNode(size_t index) const {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    return m_leafNodes[vecIndex] & (1ull << bitIndex);
}

void VarLifetimeOptimizerPass::ClearLeafNode(size_t index) {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    m_leafNodes[vecIndex] &= ~(1ull << bitIndex);
}

size_t VarLifetimeOptimizerPass::CalcMaxDistanceToLeaves(size_t nodeIndex) {
    if (m_maxDistToLeaves[nodeIndex] != ~0) {
        return m_maxDistToLeaves[nodeIndex];
    }

    size_t maxDist = 0;
    for (auto depIndex : m_fwdDeps[nodeIndex]) {
        maxDist = std::max(maxDist, CalcMaxDistanceToLeaves(depIndex) + 1);
    }
    m_maxDistToLeaves[nodeIndex] = maxDist;
    return maxDist;
}

size_t VarLifetimeOptimizerPass::CalcMaxDistanceFromRoot(size_t nodeIndex, size_t dist) {
    if (m_maxDistFromRoot[nodeIndex] > dist) {
        return m_maxDistFromRoot[nodeIndex];
    }
    m_maxDistFromRoot[nodeIndex] = dist;

    size_t maxDist = dist;
    for (auto depIndex : m_fwdDeps[nodeIndex]) {
        maxDist = std::max(maxDist, CalcMaxDistanceFromRoot(depIndex, dist + 1));
    }
    return maxDist;
}

void VarLifetimeOptimizerPass::Rewrite(size_t nodeIndex, size_t dist) {
    {
        auto nodeStr = m_ops[nodeIndex]->ToString();
        for (size_t i = 0; i < dist; i++) {
            printf("  ");
        }
        printf("  >> entering [%zu] %s\n", nodeIndex, nodeStr.c_str());
    }

    // Define function for writing nodes
    auto writeNode = [&](size_t nodeIndex) {
        // Skip already written nodes
        if (IsWrittenNode(nodeIndex)) {
            return;
        }

        // Only write the node if all of its children are already written
        for (auto depIndex : m_fwdDeps[nodeIndex]) {
            if (!IsWrittenNode(depIndex)) {
                return;
            }
        }

        {
            auto nodeStr = m_ops[nodeIndex]->ToString();
            for (size_t i = 0; i < dist; i++) {
                printf("  ");
            }
            printf("  !! rewriting [%zu] %s\n", nodeIndex, nodeStr.c_str());
        }
        m_emitter.ReinsertAtHead(m_ops[nodeIndex]);
        SetWrittenNode(nodeIndex);
    };

    // Write the current node
    writeNode(nodeIndex);

    // Move immediate parents to the head if possible
    auto &deps = m_revDeps[nodeIndex];
    for (size_t i = 0; i < deps.size(); i++) {
        const size_t depIndex = deps[deps.size() - i - 1];
        writeNode(depIndex);
    }

    // Visit parents in reverse order
    for (size_t i = 0; i < deps.size(); i++) {
        const size_t depIndex = deps[deps.size() - i - 1];
        Rewrite(depIndex, dist + 1);
    }

    {
        auto nodeStr = m_ops[nodeIndex]->ToString();
        for (size_t i = 0; i < dist; i++) {
            printf("  ");
        }
        printf("  << leaving [%zu] %s\n", nodeIndex, nodeStr.c_str());
    }
}

bool VarLifetimeOptimizerPass::IsWrittenNode(size_t index) const {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    return m_writtenNodes[vecIndex] & (1ull << bitIndex);
}

void VarLifetimeOptimizerPass::SetWrittenNode(size_t index) {
    const size_t vecIndex = index / 64;
    const size_t bitIndex = index % 64;
    m_writtenNodes[vecIndex] |= 1ull << bitIndex;
}

} // namespace armajitto::ir
