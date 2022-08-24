#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>

namespace armajitto::ir {

// Performs dead store elimination for general purpose registers.
//
// This optimization pass tracks reads and writes to GPRs and eliminates instructions that overwrite GPRs.
//
// TODO: describe further with examples
class DeadGPRStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadGPRStoreEliminationOptimizerPass(Emitter &emitter)
        : DeadStoreEliminationOptimizerPassBase(emitter) {
        m_gprWrites.fill(nullptr);
    }

private:
    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;

    // -------------------------------------------------------------------------
    // GPR read and write tracking

    std::array<IROp *, 16 * arm::kNumNormalizedModeIndices> m_gprWrites;

    void RecordGPRRead(GPRArg gpr);
    void RecordGPRWrite(GPRArg gpr, IROp *op);
};

} // namespace armajitto::ir
