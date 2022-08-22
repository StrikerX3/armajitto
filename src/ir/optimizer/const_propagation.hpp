#pragma once

#include "optimizer_pass_base.hpp"

#include <array>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Performs constant propagation and folding as well as basic instruction replacements for simple ALU and load/store
// operations.
//
// This pass propagates known values or variable assignments, eliminating as many variables as possible.
//
// This optimization pass keeps track of all assignments to variables (variables or immediate values, including those
// indirectly set through GPR assignments) and replaces known values in subsequent instructions. In some cases, the
// entire instruction is replaced with a simpler variant that directly assigns a value to a variable. The example below
// illustrates the behavior of this optimization pass:
//
//      input code             substitutions   output code          assignments
//   1  ld $v0, r0             -               ld $v0, r0           $v0 = <unknown>
//   2  lsr $v1, $v0, #0xc     -               lsr $v1, $v0, #0xc   $v1 = <unknown>
//   3  mov $v2, $v1           -               mov $v2, $v1         $v2 = $v1
//   4  st r0, $v2             $v2 -> $v1      st r0, $v1            r0 = $v1
//   5  st pc, #0x10c          -               st pc, #0x10c         pc = #0x10c
//   6  ld $v3, r0             r0 -> $v1     * copy $v3, $v1        $v3 = $v1
//   7  lsl $v4, $v3, #0xc     $v3 -> $v1      lsl $v4, $v1, #0xc   $v4 = <unknown>
//   8  mov $v5, $v4           -               mov $v5, $v4         $v5 = $v4
//   9  st r0, $v5             $v5 -> $v4      st r0, $v4            r0 = $v4
//  10  st pc, #0x110          -               st pc, #0x110         pc = #0x110
//
// The instruction marked with an asterisk indicates a replacement that may aid subsequent optimization passes.
//
// Note that some instructions in the output code can be easily eliminated by other optimization passes, such as the
// stores to unread variables $v2, $v3 and $v5 in instructions 3, 6 and 8 and the dead stores to r0 and pc in
// instructions 4 and 5 (replaced by the stores in 9 and 10).
class ConstPropagationOptimizerPass final : public OptimizerPassBase {
public:
    ConstPropagationOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

private:
    void PreProcess() final;

    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    // void Process(IRGetCPSROp *op) final;
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
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Variable substitutions

    struct Value {
        enum class Type { Unknown, Variable, Constant };
        Type type;
        union {
            Variable variable;
            uint32_t constant;
        };

        Value()
            : type(Type::Unknown) {}

        Value(Variable value)
            : type(Type::Variable)
            , variable(value) {}

        Value(uint32_t value)
            : type(Type::Constant)
            , constant(value) {}

        bool Substitute(VariableArg &var);
        bool Substitute(VarOrImmArg &var);

        bool IsKnown() const {
            return type != Type::Unknown;
        }

        bool IsConstant() const {
            return type == Type::Constant;
        }

        bool IsVariable() const {
            return type == Type::Variable;
        }
    };

    // Variable substitutions lookup table
    std::vector<Value> m_varSubsts;

    // Variable substitution operations
    void ResizeVarSubsts(size_t index);
    void Assign(VariableArg var, VariableArg value);
    void Assign(VariableArg var, ImmediateArg value);
    void Assign(VariableArg var, VarOrImmArg value);
    void Assign(VariableArg var, Variable value);
    void Assign(VariableArg var, uint32_t value);
    void Substitute(VariableArg &var);
    void Substitute(VarOrImmArg &var);

    // -------------------------------------------------------------------------
    // GPR substitutions

    // GPR substitutions lookup table
    std::array<Value, 16 * 32> m_gprSubsts;

    // GPR substitution operations
    void Assign(const GPRArg &gpr, VarOrImmArg value);
    void Forget(const GPRArg &gpr);
    Value &GetGPRSubstitution(const GPRArg &gpr);

    // -------------------------------------------------------------------------
    // PSR substitutions

    // -------------------------------------------------------------------------
    // Host flag state tracking

    // Known host flag bits
    arm::Flags m_knownHostFlagsMask = arm::Flags::None;
    arm::Flags m_knownHostFlagsValues = arm::Flags::None;

    std::optional<bool> GetCarryFlag();

    void SetKnownHostFlags(arm::Flags mask, arm::Flags values);
    void ClearKnownHostFlags(arm::Flags mask);
};

} // namespace armajitto::ir
