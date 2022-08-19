#pragma once

#include "armajitto/guest/arm/gpr.hpp"
#include "armajitto/ir/defs/variable.hpp"
#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"

#include <array>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Performs constant propagation and folding as well as basic instruction replacements for simple ALU and load/store
// operations.
//
// This pass propagates known values or variable assignments, eliminating as many variables as possible.
//
// This optimization pass keeps track of all assignments to variables (variables or immediate values) and replaces known
// values in subsequent instructions. In some cases, the entire instruction is replaced with a simpler variant that
// directly assigns a value to a variable. The example below illustrates the behavior of this optimization pass:
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
class ConstPropagationOptimizerPass {
public:
    ConstPropagationOptimizerPass(Emitter &emitter)
        : m_emitter(emitter) {}

    bool Optimize();

private:
    Emitter &m_emitter;

    void Process(IRGetRegisterOp *op);
    void Process(IRSetRegisterOp *op);
    void Process(IRGetCPSROp *op);
    void Process(IRSetCPSROp *op);
    void Process(IRGetSPSROp *op);
    void Process(IRSetSPSROp *op);
    void Process(IRMemReadOp *op);
    void Process(IRMemWriteOp *op);
    void Process(IRPreloadOp *op);
    void Process(IRLogicalShiftLeftOp *op);
    void Process(IRLogicalShiftRightOp *op);
    void Process(IRArithmeticShiftRightOp *op);
    void Process(IRRotateRightOp *op);
    void Process(IRRotateRightExtendOp *op);
    void Process(IRBitwiseAndOp *op);
    void Process(IRBitwiseOrOp *op);
    void Process(IRBitwiseXorOp *op);
    void Process(IRBitClearOp *op);
    void Process(IRCountLeadingZerosOp *op);
    void Process(IRAddOp *op);
    void Process(IRAddCarryOp *op);
    void Process(IRSubtractOp *op);
    void Process(IRSubtractCarryOp *op);
    void Process(IRMoveOp *op);
    void Process(IRMoveNegatedOp *op);
    void Process(IRSaturatingAddOp *op);
    void Process(IRSaturatingSubtractOp *op);
    void Process(IRMultiplyOp *op);
    void Process(IRMultiplyLongOp *op);
    void Process(IRAddLongOp *op);
    void Process(IRStoreFlagsOp *op);
    void Process(IRUpdateFlagsOp *op);
    void Process(IRUpdateStickyOverflowOp *op);
    void Process(IRBranchOp *op);
    void Process(IRBranchExchangeOp *op);
    void Process(IRLoadCopRegisterOp *op);
    void Process(IRStoreCopRegisterOp *op);
    void Process(IRConstantOp *op);
    void Process(IRCopyVarOp *op);
    void Process(IRGetBaseVectorAddressOp *op);

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

        void Substitute(VariableArg &var);
        void Substitute(VarOrImmArg &var);

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

    // Note: this is only concerned with the flags bits; the rest of the variable's value doesn't matter
    struct FlagsValue {
        Flags knownMask;
        Flags flags;

        FlagsValue()
            : knownMask(Flags::None)
            , flags(Flags::None) {}

        FlagsValue(Flags mask, Flags flags)
            : knownMask(mask)
            , flags(flags) {}
    };

    // Lookup variable or GPR in these lists to find out what its substitution is, if any.
    std::vector<Value> m_varSubsts;
    std::array<Value, 16> m_gprSubsts;
    std::array<Value, 16> m_userGPRSubsts;

    Flags m_knownFlagsMask = Flags::None;
    Flags m_knownFlagsValues = Flags::None;
    std::vector<FlagsValue> m_flagsSubsts;

    std::optional<bool> GetCarryFlag();

    // General variable substitutions
    void ResizeVarSubsts(size_t size);
    void Assign(VariableArg var, VariableArg value);
    void Assign(VariableArg var, ImmediateArg value);
    void Assign(VariableArg var, VarOrImmArg value);
    void Assign(VariableArg var, Variable value);
    void Assign(VariableArg var, uint32_t value);
    void Substitute(VariableArg &var);
    void Substitute(VarOrImmArg &var);

    // GPR substitutions
    void Assign(GPRArg gpr, VarOrImmArg value);
    void Forget(GPRArg gpr);
    Value &GetGPRSubstitution(GPRArg gpr);

    // Flags substitutions
    void ResizeFlagsSubsts(size_t size);
    void Assign(VariableArg var, Flags mask, Flags flags);
    FlagsValue *GetFlagsSubstitution(VariableArg var);
};

} // namespace armajitto::ir
