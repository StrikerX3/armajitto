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

    struct OpRef {
        IROp *op = nullptr;
        size_t index = ~0;
    };

    struct AccessRecord {
        OpRef read;
        OpRef write;
    };

    size_t m_opIndex = 0;

    std::pmr::vector<AccessRecord> m_varAccesses;
    alignas(16) std::array<AccessRecord, 16 * arm::kNumBankedModes> m_gprAccesses;
    alignas(16) std::array<AccessRecord, 1 + arm::kNumBankedModes> m_psrAccesses; // 0=CPSR, 1..6=SPSR by mode
    AccessRecord m_flagNAccesses;
    AccessRecord m_flagZAccesses;
    AccessRecord m_flagCAccesses;
    AccessRecord m_flagVAccesses;

    void ResizeVarAccesses(size_t index);

    void RecordRead(IROp *op, VarOrImmArg arg);
    void RecordRead(IROp *op, VariableArg arg);
    void RecordRead(IROp *op, GPRArg arg);
    void RecordCPSRRead(IROp *op);
    void RecordSPSRRead(IROp *op, arm::Mode mode);
    void RecordPSRRead(IROp *op, size_t index);
    void RecordRead(IROp *op, arm::Flags flags);

    void RecordWrite(IROp *op, VarOrImmArg arg);
    void RecordWrite(IROp *op, VariableArg arg);
    void RecordWrite(IROp *op, GPRArg arg);
    void RecordCPSRWrite(IROp *op);
    void RecordSPSRWrite(IROp *op, arm::Mode mode);
    void RecordPSRWrite(IROp *op, size_t index);
    void RecordWrite(IROp *op, arm::Flags flags);

    // -------------------------------------------------------------------------
    // Dependency graph

    std::pmr::vector<uint64_t> m_rootNodes;                    // bit vector
    std::pmr::vector<std::pmr::vector<size_t>> m_dependencies; // deps[from] -> {to, to, to}; duplicates are harmless
    std::pmr::vector<size_t> m_rootNodeOrder;
    std::pmr::vector<size_t> m_maxDistances; // root: distance to furthest node; non-root: max distance from root

    void AddReadDependencyEdge(IROp *op, AccessRecord &record);
    void AddWriteDependencyEdge(IROp *op, AccessRecord &record);

    void AddEdge(size_t from, size_t to);

    size_t CalcMaxDistance(size_t nodeIndex, size_t totalDist = 1);
};

} // namespace armajitto::ir
