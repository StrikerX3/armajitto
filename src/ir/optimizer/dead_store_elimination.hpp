#pragma once

#include "optimizer_pass_base.hpp"

#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for variables, registers, PSRs and flags.
//
// This optimization pass tracks reads and writes to variables and eliminates variables that are never read.
// Instructions that end up writing to no variables are removed from the code, unless they have side effects such as
// updating the host flags, changing the values of GPRs or PSRs, writing to memory or reading from memory with side
// effects (for instance, MMIO regions).
//
// Assuming the following IR code fragment:
//     instruction
//  1  ld $v0, r0
//  2  lsr $v1, $v0, #0xc
//  3  mov $v2, $v1
//  4  st r0, $v1
//  5  st pc, #0x10c
//
// The algorithm keeps track of variables and GPRs written to, pointing to the instruction that last wrote to them.
//
//     instruction              writes
//  1  ld $v0, r0               $v0
//  2  lsr $v1, $v0, #0xc       $v1
//  3  mov $v2, $v1             $v2
//  4  st r0, $v1               r0
//  5  st pc, #0x10c            pc
//
// Whenever a variable is read, the corresponding write is marked as "read". This clears the pointer to the instruction
// for that particular variable. If the variable is used in an instruction that produces side effects, it is also marked
// as "consumed". Consumed variables are denoted in parentheses in the listings below.
//
//     instruction              writes   reads   actions
//  1  ld $v0, r0               ($v0)
//  2  lsr $v1, $v0, #0xc       ($v1)    $v0     marks the write to $v0 in instruction 1 as consumed
//  3  mov $v2, $v1             $v2      $v1     marks the write to $v1 in instruction 2 as consumed
//  4  st r0, $v1               r0       $v1     $v1 no longer has a write to check for, so nothing is done
//  5  st pc, #0x10c            pc
//
// When a variable, GPR, PSR or flag is overwritten before being read, the original destination argument is marked as
// unused. If the instruction has no used writes and no side effects (writes to host flags, GPRs or PSRs), it is
// removed. Note that this does not preclude the removal of instructions that write to GPRs or PSRs, as removing the
// write to the GPR also removes that side effect. To illustrate this, let's expand the example above with more code:
//
//     instruction              writes   reads   actions
//  1  ld $v0, r0               ($v0)
//  2  lsr $v1, $v0, #0xc       ($v1)    $v0     marks the write to $v0 in instruction 1 as consumed
//  3  mov $v2, $v1             $v2      $v1     marks the write to $v1 in instruction 2 as consumed
//  4  st r0, $v1               r0       $v1     (erased by instruction 9)
//  5  st pc, #0x10c            pc               (erased by instruction 10)
//  6  copy $v3, $v1            $v3      $v1     again, nothing is done because $v1 has no write to mark
//  7  lsl $v4, $v1, #0xc       ($v4)    $v1     same as above
//  8  mov $v5, $v4             $v5      $v4     marks the write to $v4 in instruction 7 as consumed
//  9  st r0, $v4               r0       $v4     write to r0 from instruction 4 is unused
//                                                 instruction 4 has no writes or side effects, so erase it
// 10  st pc, #0x110            pc               write to pc from instruction 5 is unused
//                                                 instruction 5 has no writes or side effects, so erase it
//
// At the end of the block, any unread writes are marked so and if the corresponding instructions no longer have any
// writes or side effects, they are also removed. In the listing above, instructions 3, 6 and 8 write to variables $v2,
// $v3 and $v5 which are never read, thus leaving the instructions useless. Writes to unread GPRs, PSRs and flags are
// kept, unless overwritten. The resulting code after the optimization is:
//
//  1  ld $v0, r0
//  2  lsr $v1, $v0, #0xc
//  3  lsl $v4, $v1, #0xc
//  4  st r0, $v4
//  5  st pc, #0x110
//
// In addition to keeping track of reads and writes as described above, the algorithm also tracks the dependencies
// between variables in order to eliminate entire sequences of dead stores, such as in the following example:
//
//     instruction            dependency chains
//  1  ld $v0, r0             $v0
//  2  lsr $v1, $v0, #0xc     $v1 -> $v0
//  3  copy $v2, $v1          $v2 -> $v1 -> $v0
//  4  copy $v3, $v2          $v3 -> $v2 -> $v1 -> $v0
//  5  copy $v4, $v3          $v4 -> $v3 -> $v2 -> $v1 -> $v0
//  6  st r0, $v1             $v4 -> $v3 -> $v2
//                            (consumes $v1, breaking the dependency between $v2 and $v1)
//
// Operations that read from a variable and store a result in another variable create a dependency between the written
// and read variable. The chain is broken if a variable is consumed, as described earlier.
//
// Without this, the optimizer would require multiple passes to remove instructions 3, 4 and 5 since $v2 and $v3 are
// read by the following instructions, but never really used. By tracking dependency chains, the optimizer can erase all
// three instructions in one go once it reaches the end of the block by simply following the chain when erasing writes.
//
// The only IR instructions that read but do not consume a variable are "copy" (IRCopyVarOp) and "mov" (IRMoveVarOp) if
// it doesn't set flags.
//
// Note that the above sequence is impossible if the constant propagation pass is applied before this pass as the right
// hand side arguments for instructions 4 and 5 would be replaced with $v1.
// It is also impossible for a variable to be written to more than once thanks to the SSA form. However, some
// instructions may link one write to multiple input variables, such as the mull and addl instructions:
//
//    ld $v0, r0
//    ld $v1, r1
//    umull $v2, $v3, $v0, $v1             $v2 -> [$v0, $v1]; $v3 -> [$v0, $v1]
//    addl $v4, $v5, $v0, $v1, $v2, $v3    $v4 -> [$v0, $v1, $v2, $v3]; $v5 -> [$v0, $v1, $v2, $v3]  (+ both above)
//
// In those cases, the optimizer will follow every linked variable and erase all affected instructions.
class DeadStoreEliminationOptimizerPass final : public OptimizerPassBase {
public:
    DeadStoreEliminationOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

private:
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
    void Process(IRRotateRightExtendOp *op) final;
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
    void Process(IRUpdateFlagsOp *op) final;
    void Process(IRUpdateStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    void Process(IRGetBaseVectorAddressOp *op) final;

    void RecordWrite(Variable dst, IROp *op);
    void RecordWrite(VariableArg dst, IROp *op);

    void RecordRead(Variable dst, bool consume = true);
    void RecordRead(VariableArg dst, bool consume = true);
    void RecordRead(VarOrImmArg dst, bool consume = true);

    void RecordDependentRead(Variable dst, Variable src, bool consume = true);
    void RecordDependentRead(VariableArg dst, Variable src, bool consume = true);
    void RecordDependentRead(Variable dst, VariableArg src, bool consume = true);
    void RecordDependentRead(VariableArg dst, VariableArg src, bool consume = true);
    void RecordDependentRead(Variable dst, VarOrImmArg src, bool consume = true);
    void RecordDependentRead(VariableArg dst, VarOrImmArg src, bool consume = true);

    void ResizeWrites(size_t size);
    void ResizeDependencies(size_t size);

    void EraseWriteRecursive(Variable var, IROp *op);

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    bool EraseWrite(Variable var, T *op) {
        return false;
    }

    bool EraseWrite(Variable var, IRGetRegisterOp *op);
    // bool EraseWrite(GPRArg var, IRSetRegisterOp *op);
    bool EraseWrite(Variable var, IRGetCPSROp *op);
    // bool EraseWrite(IRSetCPSROp *op);
    bool EraseWrite(Variable var, IRGetSPSROp *op);
    // bool EraseWrite(IRSetSPSROp *op);
    bool EraseWrite(Variable var, IRMemReadOp *op);
    // IRMemWriteOp has no writes
    // IRPreloadOp has no writes
    bool EraseWrite(Variable var, IRLogicalShiftLeftOp *op);
    bool EraseWrite(Variable var, IRLogicalShiftRightOp *op);
    bool EraseWrite(Variable var, IRArithmeticShiftRightOp *op);
    bool EraseWrite(Variable var, IRRotateRightOp *op);
    bool EraseWrite(Variable var, IRRotateRightExtendOp *op);
    bool EraseWrite(Variable var, IRBitwiseAndOp *op);
    bool EraseWrite(Variable var, IRBitwiseOrOp *op);
    bool EraseWrite(Variable var, IRBitwiseXorOp *op);
    bool EraseWrite(Variable var, IRBitClearOp *op);
    bool EraseWrite(Variable var, IRCountLeadingZerosOp *op);
    bool EraseWrite(Variable var, IRAddOp *op);
    bool EraseWrite(Variable var, IRAddCarryOp *op);
    bool EraseWrite(Variable var, IRSubtractOp *op);
    bool EraseWrite(Variable var, IRSubtractCarryOp *op);
    bool EraseWrite(Variable var, IRMoveOp *op);
    bool EraseWrite(Variable var, IRMoveNegatedOp *op);
    bool EraseWrite(Variable var, IRSaturatingAddOp *op);
    bool EraseWrite(Variable var, IRSaturatingSubtractOp *op);
    bool EraseWrite(Variable var, IRMultiplyOp *op);
    bool EraseWrite(Variable var, IRMultiplyLongOp *op);
    bool EraseWrite(Variable var, IRAddLongOp *op);
    bool EraseWrite(Variable var, IRStoreFlagsOp *op);
    bool EraseWrite(Variable var, IRUpdateFlagsOp *op);
    bool EraseWrite(Variable var, IRUpdateStickyOverflowOp *op);
    // bool EraseWrite(IRBranchOp *op); // Writes PC
    // bool EraseWrite(IRBranchExchangeOp *op); // Writes PC and CPSR
    bool EraseWrite(Variable var, IRLoadCopRegisterOp *op);
    // IRStoreCopRegisterOp has no writes
    bool EraseWrite(Variable var, IRConstantOp *op);
    bool EraseWrite(Variable var, IRCopyVarOp *op);
    bool EraseWrite(Variable var, IRGetBaseVectorAddressOp *op);

    struct VarWrite {
        IROp *op = nullptr;
        bool read = false;
        bool consumed = false;
    };

    std::vector<VarWrite> m_varWrites;
    std::vector<std::vector<Variable>> m_dependencies;
};

} // namespace armajitto::ir
