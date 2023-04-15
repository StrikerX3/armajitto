#pragma once

#include "optimizer_pass_base.hpp"

#include <array>
#include <memory_resource>
#include <vector>

namespace armajitto::ir {

// Optimizes variable lifetimes.
//
// (TODO: describe algorithm)
//
// (TODO: demonstrate algorithm with an example)
class VarLifetimeOptimizerPass final : public OptimizerPassBase {
public:
    VarLifetimeOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc);

private:
    void Reset() final;

    void PostProcess(IROp *op) final;

    void PostProcess() final;

    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    void Process(IRGetSPSROp *op) final;
    void Process(IRSetSPSROp *op) final;
    void Process(IRMemReadOp *op) final;
    void Process(IRMemWriteOp *op) final;
    void Process(IRPreloadOp *op) final;
    void Process(IRLogicalShiftLeftOp *op) final;
    void Process(IRLogicalShiftRightOp *op) final;
    void Process(IRArithmeticShiftRightOp *op) final;
    void Process(IRRotateRightOp *op) final;
    void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    void Process(IRCountLeadingZerosOp *op) final;
    void Process(IRAddOp *op) final;
    void Process(IRAddCarryOp *op) final;
    void Process(IRSubtractOp *op) final;
    void Process(IRSubtractCarryOp *op) final;
    void Process(IRMoveOp *op) final;
    void Process(IRMoveNegatedOp *op) final;
    void Process(IRSaturatingAddOp *op) final;
    void Process(IRSaturatingSubtractOp *op) final;
    void Process(IRMultiplyOp *op) final;
    void Process(IRMultiplyLongOp *op) final;
    void Process(IRAddLongOp *op) final;
    void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Read/write tracking

    struct AccessRecord {
        size_t readIndex = ~0;
        size_t writeIndex = ~0;
    };

    size_t m_opIndex = 0;

    std::pmr::vector<IROp *> m_ops;
    std::pmr::vector<AccessRecord> m_varAccesses;
    alignas(16) std::array<AccessRecord, 16 * arm::kNumBankedModes> m_gprAccesses;
    alignas(16) std::array<AccessRecord, 1 + arm::kNumBankedModes> m_psrAccesses; // 0=CPSR, 1..6=SPSR by mode
    AccessRecord m_flagNAccesses;
    AccessRecord m_flagZAccesses;
    AccessRecord m_flagCAccesses;
    AccessRecord m_flagVAccesses;

    void ResizeVarAccesses(size_t index);

    void RecordRead(VarOrImmArg arg);
    void RecordRead(VariableArg arg);
    void RecordRead(GPRArg arg);
    void RecordCPSRRead();
    void RecordSPSRRead(arm::Mode mode);
    void RecordPSRRead(size_t index);
    void RecordRead(arm::Flags flags);

    void RecordWrite(VarOrImmArg arg);
    void RecordWrite(VariableArg arg);
    void RecordWrite(GPRArg arg);
    void RecordCPSRWrite();
    void RecordSPSRWrite(arm::Mode mode);
    void RecordPSRWrite(size_t index);
    void RecordWrite(arm::Flags flags);

    // -------------------------------------------------------------------------
    // Dependency graph

    std::pmr::vector<uint64_t> m_rootNodes;               // bit vector
    std::pmr::vector<uint64_t> m_leafNodes;               // bit vector
    std::pmr::vector<std::pmr::vector<size_t>> m_fwdDeps; // deps[from] -> {to, to, ...}; sorted, no dupes
    std::pmr::vector<std::pmr::vector<size_t>> m_revDeps; // deps[to] -> {from, from, ...}; sorted, no dupes
    std::pmr::vector<size_t> m_sortedLeafNodes;
    std::pmr::vector<size_t> m_maxDistToLeaves; // distance to furthest node
    std::pmr::vector<size_t> m_maxDistFromRoot; // max distance from root
    std::pmr::vector<uint64_t> m_writtenNodes;  // bit vector indicating which nodes have been written

    void AddReadDependencyEdge(AccessRecord &record);
    void AddWriteDependencyEdge(AccessRecord &record);

    void AddEdge(size_t from, size_t to);

    bool IsRootNode(size_t index) const;
    void ClearRootNode(size_t index);

    bool IsLeafNode(size_t index) const;
    void ClearLeafNode(size_t index);

    size_t CalcMaxDistanceToLeaves(size_t nodeIndex);
    size_t CalcMaxDistanceFromRoot(size_t nodeIndex, size_t dist = 1);

    void Rewrite(size_t nodeIndex, size_t dist = 0);

    bool IsWrittenNode(size_t index) const;
    void SetWrittenNode(size_t index);
};

} // namespace armajitto::ir
