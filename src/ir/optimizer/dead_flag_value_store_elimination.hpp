#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <memory_resource>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for flag values in variables.
//
// The algorithm tracks the last instructions that wrote to each one of the NZCV flags in variables.
// It only tracks the AND, ORR, BIC bitwise operations with a variable and an immediate argument and the load CPSR, load
// flags and load sticky overflow flag instructions.
//
// Loading from CPSR initializes the variable into an unknown state. This variable is used as the "base" for subsequent
// operations. Each operation that takes this variable and outputs another variable connects those two in a chain and
// erases the written flags from the previous instructions -- from the immediate values for the bitwise operations, or
// from the flags mask for the load flags instructions. This is done per flag.
//
// Assuming the following IR code fragment:
//  #  instruction
//  1  ld $v0, cpsr
//  2  bic $v1, $v0, #0xc0000000
//  3  orr $v2, $v1, #0x78000000
//  4  ldflg.q $v3, $v2
//  5  ldflg.nc $v4, $v3
//  6  st cpsr, $v4
//
// The algorithm takes the following actions for each instruction:
//  1. Records $v0 as the base of a series of flag value modifications.
//  2. Stores this instruction as the writer for flags NZ (corresponding to #0xc0000000) into the base variable $v0.
//  3. Erases the Z write from instruction 2, modifying its immediate value to #0x80000000.
//     Stores this instruction as the writer for flags ZCV into the base variable $v0.
//  4. No action taken.
//  5. Erases the N write from instruction 2, modifying its immediate value to #0x00000000.
//     Erases the C write from instruction 3, modifying its immediate value to #0x50000000.
//     Stores this instruction as the writer for flags NZ into the base variable $v0.
//  6. No action taken.
//
// The resulting code is:
//  #  instruction
//  1  ld $v0, cpsr
//  2  bic $v1, $v0, #0x00000000
//  3  orr $v2, $v1, #0x58000000
//  4  ldflg.q $v3, $v2
//  5  ldflg.nc $v4, $v3
//  6  st cpsr, $v4
//
// The BIC operation becomes an identity operation, which is removed by a later optimization pass.
class DeadFlagValueStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadFlagValueStoreEliminationOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc);

private:
    void Reset() final;

    // void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    // void Process(IRGetSPSROp *op) final;
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
    void Process(IRSignExtendHalfOp *op) final;
    void Process(IRSaturatingAddOp *op) final;
    void Process(IRSaturatingSubtractOp *op) final;
    void Process(IRMultiplyOp *op) final;
    void Process(IRMultiplyLongOp *op) final;
    void Process(IRAddLongOp *op) final;
    void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    // void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    // void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Flags tracking

    struct FlagWrites {
        Variable base = {}; // the base variable from which this chain originates

        IROp *writerOpN = nullptr; // last instruction that wrote to N
        IROp *writerOpZ = nullptr; // last instruction that wrote to Z
        IROp *writerOpC = nullptr; // last instruction that wrote to C
        IROp *writerOpV = nullptr; // last instruction that wrote to V
    };

    std::pmr::vector<FlagWrites> m_flagWritesPerVar;

    void ResizeFlagWritesPerVar(size_t index);
    void InitFlagWrites(VariableArg base);
    void RecordFlagWrites(VariableArg dst, VariableArg src, arm::Flags flags, IROp *writerOp);
    void ConsumeFlags(VariableArg &arg);
    void ConsumeFlags(VarOrImmArg &arg);

    // -------------------------------------------------------------------------
    // Generic EraseFlagWrite

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    void EraseFlagWrite(arm::Flags flag, T *op) {}

    void EraseFlagWrite(arm::Flags flag, IRBitwiseAndOp *op);
    void EraseFlagWrite(arm::Flags flag, IRBitwiseOrOp *op);
    void EraseFlagWrite(arm::Flags flag, IRBitClearOp *op);
    void EraseFlagWrite(arm::Flags flag, IRLoadFlagsOp *op);
    void EraseFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op);
};

} // namespace armajitto::ir
