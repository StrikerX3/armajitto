#include "var_lifetime_opt.hpp"

namespace armajitto::ir {

VarLifetimeOptimizerPass::VarLifetimeOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc)
    : OptimizerPassBase(emitter)
    , m_varAccesses(&alloc)
    , m_rootNodes(&alloc)
    , m_dependencies(&alloc)
    , m_sortedRootNodes(&alloc)
    , m_maxDistToLeaves(&alloc)
    , m_maxDistFromRoot(&alloc) {}

void VarLifetimeOptimizerPass::Reset() {
    AccessRecord empty{};

    const uint32_t varCount = m_emitter.VariableCount();
    m_varAccesses.resize(varCount);

    const size_t opCount = m_emitter.IROpCount();
    m_rootNodes.resize((opCount + 63) / 64);
    m_dependencies.resize(opCount);
    m_maxDistToLeaves.resize(opCount);
    m_maxDistFromRoot.resize(opCount);

    m_opIndex = 0;

    std::fill(m_varAccesses.begin(), m_varAccesses.end(), empty);
    m_gprAccesses.fill(empty);
    m_psrAccesses.fill(empty);
    m_flagNAccesses = empty;
    m_flagZAccesses = empty;
    m_flagCAccesses = empty;
    m_flagVAccesses = empty;

    std::fill(m_rootNodes.begin(), m_rootNodes.end(), ~0ull);
    std::fill(m_maxDistToLeaves.begin(), m_maxDistToLeaves.end(), ~0);
    std::fill(m_maxDistFromRoot.begin(), m_maxDistFromRoot.end(), 0);
    for (auto &deps : m_dependencies) {
        deps.clear();
    }
}

void VarLifetimeOptimizerPass::PostProcess(IROp *op) {
    ++m_opIndex;
}

void VarLifetimeOptimizerPass::PostProcess() {
    if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466) {
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
        const size_t opCount = m_maxDistToLeaves.size();
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

        printf("dependencies:\n");
        for (size_t i = 0; i < m_dependencies.size(); i++) {
            if (!m_dependencies[i].empty()) {
                printf("%zu:", i);
                for (auto dep : m_dependencies[i]) {
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
    m_sortedRootNodes.clear();
    for (size_t i = 0; i < m_rootNodes.size(); i++) {
        uint64_t bitmap = m_rootNodes[i];
        size_t bmindex = std::countr_zero(bitmap);
        while (bmindex < 64) {
            const size_t rootIndex = bmindex + i * 64;
            if (rootIndex >= opCount) {
                break;
            }
            m_sortedRootNodes.push_back(rootIndex);

            // Traverse all children nodes, computing maximum distances from root
            for (auto node : m_dependencies[rootIndex]) {
                CalcMaxDistanceFromRoot(node);
            }

            bitmap &= ~(1ull << bmindex);
            bmindex = std::countr_zero(bitmap);
        }
    }

    if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466) {
        printf("max distances:\n");
        for (size_t i = 0; i < m_maxDistToLeaves.size(); i++) {
            printf("  %zu = %zu // %zu", i, m_maxDistToLeaves[i], m_maxDistFromRoot[i]);
            if (IsRootNode(i)) {
                printf(" (root)");
            }
            printf("\n");
        }
        printf("\n");
    }

    // Sort root nodes from longest to shortest, then highest to lowest index for stability over multiple executions
    std::sort(m_sortedRootNodes.begin(), m_sortedRootNodes.end(), [&](uint64_t lhs, uint64_t rhs) {
        if (m_maxDistToLeaves[lhs] > m_maxDistToLeaves[rhs]) {
            return true;
        }
        if (m_maxDistToLeaves[lhs] < m_maxDistToLeaves[rhs]) {
            return false;
        }

        return lhs > rhs;
    });

    if (m_emitter.GetBlock().Location().PC() == 0xFFFF0466) {
        printf("sorted root nodes:");
        for (auto index : m_sortedRootNodes) {
            printf(" %zu", index);
        }
        printf("\n");
    }

    // TODO: initialize bit vector of written nodes
    // TODO: iterate over sorted nodes and visit every child node, depth first:
    // - track distance from root to that node
    // - if every child of the visited node has been written and the current distance matches the visited node's
    //   distance:
    //   - prepend the node to the beginning of the block
    //   - mark node as written

    // TODO: implement instruction reordering
}

void VarLifetimeOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordRead(op, op->src);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op, op->src);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead(op);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op, op->src);
    RecordCPSRWrite(op);
}

void VarLifetimeOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op, op->mode);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op, op->src);
    RecordSPSRWrite(op, op->mode);
}

void VarLifetimeOptimizerPass::Process(IRMemReadOp *op) {
    RecordRead(op, op->address);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op, op->address);
    RecordRead(op, op->src);
}

void VarLifetimeOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op, op->address);
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordRead(op, op->value);
    RecordRead(op, op->amount);
    RecordWrite(op, op->dst);
    if (op->setCarry) {
        RecordWrite(op, arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordRead(op, op->value);
    RecordRead(op, op->amount);
    RecordWrite(op, op->dst);
    if (op->setCarry) {
        RecordWrite(op, arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordRead(op, op->value);
    RecordRead(op, op->amount);
    RecordWrite(op, op->dst);
    if (op->setCarry) {
        RecordWrite(op, arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightOp *op) {
    RecordRead(op, op->value);
    RecordRead(op, op->amount);
    RecordWrite(op, op->dst);
    if (op->setCarry) {
        RecordWrite(op, arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    RecordRead(op, op->value);
    RecordRead(op, arm::Flags::C);
    RecordWrite(op, op->dst);
    if (op->setCarry) {
        RecordWrite(op, arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitClearOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordRead(op, op->value);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRAddOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddCarryOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordRead(op, arm::Flags::C);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordRead(op, arm::Flags::C);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveOp *op) {
    RecordRead(op, op->value);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordRead(op, op->value);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dst);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordRead(op, op->lhs);
    RecordRead(op, op->rhs);
    RecordWrite(op, op->dstLo);
    RecordWrite(op, op->dstHi);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddLongOp *op) {
    RecordRead(op, op->lhsLo);
    RecordRead(op, op->lhsHi);
    RecordRead(op, op->rhsLo);
    RecordRead(op, op->rhsHi);
    RecordWrite(op, op->dstLo);
    RecordWrite(op, op->dstHi);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordRead(op, op->values);
    RecordWrite(op, op->flags);
}

void VarLifetimeOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordRead(op, op->srcCPSR);
    RecordRead(op, op->flags);
    RecordWrite(op, op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    RecordRead(op, op->srcCPSR);
    if (op->setQ) {
        RecordRead(op, arm::Flags::V);
    }
    RecordWrite(op, op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op, op->address);
    RecordCPSRRead(op);
    RecordWrite(op, arm::GPR::PC);
}

void VarLifetimeOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op, op->address);
    RecordCPSRRead(op);
    RecordWrite(op, arm::GPR::PC);
    RecordCPSRWrite(op);
}

void VarLifetimeOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    RecordWrite(op, op->dstValue);
}

void VarLifetimeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op, op->srcValue);
}

void VarLifetimeOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRCopyVarOp *op) {
    RecordRead(op, op->var);
    RecordWrite(op, op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op, op->dst);
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

void VarLifetimeOptimizerPass::RecordRead(IROp *op, VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordRead(op, arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordRead(IROp *op, VariableArg arg) {
    if (!arg.var.IsPresent()) {
        return;
    }

    const auto varIndex = arg.var.Index();
    ResizeVarAccesses(varIndex);
    AddReadDependencyEdge(op, m_varAccesses[varIndex]);
}

void VarLifetimeOptimizerPass::RecordRead(IROp *op, GPRArg arg) {
    AddReadDependencyEdge(op, m_gprAccesses[arg.Index()]);
}

void VarLifetimeOptimizerPass::RecordCPSRRead(IROp *op) {
    RecordPSRRead(op, 0);
}

void VarLifetimeOptimizerPass::RecordSPSRRead(IROp *op, arm::Mode mode) {
    RecordPSRRead(op, SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRRead(IROp *op, size_t index) {
    AddReadDependencyEdge(op, m_psrAccesses[index]);
}

void VarLifetimeOptimizerPass::RecordRead(IROp *op, arm::Flags flags) {
    const auto bmFlags = BitmaskEnum(flags);
    auto update = [&](arm::Flags flag, AccessRecord &accessRecord) {
        if (bmFlags.AnyOf(flag)) {
            AddReadDependencyEdge(op, accessRecord);
        }
    };
    update(arm::Flags::N, m_flagNAccesses);
    update(arm::Flags::Z, m_flagZAccesses);
    update(arm::Flags::C, m_flagCAccesses);
    update(arm::Flags::V, m_flagVAccesses);
}

void VarLifetimeOptimizerPass::RecordWrite(IROp *op, VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordWrite(op, arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordWrite(IROp *op, VariableArg arg) {
    if (!arg.var.IsPresent()) {
        return;
    }

    const auto varIndex = arg.var.Index();
    ResizeVarAccesses(varIndex);
    AddWriteDependencyEdge(op, m_varAccesses[varIndex]);
}

void VarLifetimeOptimizerPass::RecordWrite(IROp *op, GPRArg arg) {
    AddWriteDependencyEdge(op, m_gprAccesses[arg.Index()]);
}

void VarLifetimeOptimizerPass::RecordCPSRWrite(IROp *op) {
    RecordPSRWrite(op, 0);
}

void VarLifetimeOptimizerPass::RecordSPSRWrite(IROp *op, arm::Mode mode) {
    RecordPSRWrite(op, SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRWrite(IROp *op, size_t index) {
    AddWriteDependencyEdge(op, m_psrAccesses[index]);
}

void VarLifetimeOptimizerPass::RecordWrite(IROp *op, arm::Flags flags) {
    const auto bmFlags = BitmaskEnum(flags);
    auto update = [&](arm::Flags flag, AccessRecord &accessRecord) {
        if (bmFlags.AnyOf(flag)) {
            AddWriteDependencyEdge(op, accessRecord);
        }
    };
    update(arm::Flags::N, m_flagNAccesses);
    update(arm::Flags::Z, m_flagZAccesses);
    update(arm::Flags::C, m_flagCAccesses);
    update(arm::Flags::V, m_flagVAccesses);
}

// ---------------------------------------------------------------------------------------------------------------------
// Dependency graph

void VarLifetimeOptimizerPass::AddReadDependencyEdge(IROp *op, AccessRecord &record) {
    if (record.write.op != nullptr) {
        AddEdge(record.write.index, m_opIndex);
    }
    record.read.op = op;
    record.read.index = m_opIndex;
}

void VarLifetimeOptimizerPass::AddWriteDependencyEdge(IROp *op, AccessRecord &record) {
    if (record.read.op != nullptr) {
        AddEdge(record.read.index, m_opIndex);
    }
    if (record.write.op != nullptr) {
        AddEdge(record.write.index, m_opIndex);
    }
    record.write.op = op;
    record.write.index = m_opIndex;
}

void VarLifetimeOptimizerPass::AddEdge(size_t from, size_t to) {
    // Don't add self-dependencies
    if (from == to) {
        return;
    }

    // Don't repeat the same edge
    auto &deps = m_dependencies[from];
    if (!deps.empty() && deps.back() == to) {
        return;
    }

    // Add edge to graph
    deps.push_back(to);

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

size_t VarLifetimeOptimizerPass::CalcMaxDistanceToLeaves(size_t nodeIndex) {
    if (m_maxDistToLeaves[nodeIndex] != ~0) {
        return m_maxDistToLeaves[nodeIndex];
    }

    size_t maxDist = 0;
    for (auto depIndex : m_dependencies[nodeIndex]) {
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
    for (auto depIndex : m_dependencies[nodeIndex]) {
        maxDist = std::max(maxDist, CalcMaxDistanceFromRoot(depIndex, dist + 1));
    }
    return maxDist;
}

} // namespace armajitto::ir
