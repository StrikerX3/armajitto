#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>

namespace armajitto::ir {

// Performs dead store elimination for general purpose registers.
//
// This optimization pass tracks reads and writes to GPRs and eliminates instructions that overwrite GPRs.
//
// The algorithm simply tracks the last write to a GPR (per GPR, per mode). If the GPR is read, the last write
// instruction is left alone. If the GPR is written multiple times, the previous write instructions are erased.
//
// Assuming the following IR code fragment:
//  #  instruction
//  1  ld $v0, r0
//  2  add $v1, $v0, 5
//  3  st r0, $v1
//  4  ld $v2, r4
//  5  st r0, $v2
//  6  ld $v3, r0
//  7  add $v4, $v3, 6
//  8  st r0, $v4
//
// The algorithm takes the following actions for each instruction:
//  1. No action taken -- there is no previous write to R0.
//  2. No action taken -- not a GPR load/store.
//  3. This instruction is recorded as the last instruction that wrote to R0.
//  4. No action taken -- there is no previous write to R4.
//  5. Erases instruction 3 -- R0 is overwritten.
//  6. Consumes R0 -- instruction 5 is no longer marked as the previous write to R0.
//  7. No action taken -- not a GPR load/store.
//  8. This instruction is recorded as the last instruction that wrote to R0.
class DeadGPRStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadGPRStoreEliminationOptimizerPass(Emitter &emitter);

private:
    void Reset() final;

    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;

    // -------------------------------------------------------------------------
    // GPR read and write tracking

    std::array<IROp *, 16 * arm::kNumBankedModes> m_gprWrites;

    void RecordGPRRead(GPRArg gpr);
    void RecordGPRWrite(GPRArg gpr, IROp *op);
};

} // namespace armajitto::ir
