#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <memory_resource>
#include <vector>

namespace armajitto::ir {

// Helper class to perform variable substitutions.
// Add an instance of this class to an optimizer, then add this to the IROp preprocessor stage:
//   m_varSubst.Substitute(op);
// Invoke Assign to assign variable substitutions.
//
// Does not work on a backward scan.
class VarSubstitutor {
public:
    VarSubstitutor(size_t varCount, std::pmr::memory_resource &alloc);

    // Assigns a substitution of all <src> variables to <dst>.
    void Assign(VariableArg dst, VariableArg src);

    // Substitutes the variables in the specified IROp. Returns true if any substitution took place.
    bool Substitute(IROp *op);

private:
    std::pmr::vector<Variable> m_varSubsts;

    void ResizeVarSubsts(size_t index);

    // Substitutes the specified variable in-place if a substitution exists.
    // Returns true if a substitution took place.
    bool Substitute(VariableArg &var);

    // Substitutes the specified variable in-place if a substitution exists and the argument is a variable.
    // Returns true if a substitution took place.
    bool Substitute(VarOrImmArg &var);

    // Catch-all method for unused ops, required by the visitor.
    template <typename T>
    bool SubstituteImpl(T *op) {
        return false;
    }

    bool SubstituteImpl(IRSetRegisterOp *op);
    bool SubstituteImpl(IRSetCPSROp *op);
    bool SubstituteImpl(IRSetSPSROp *op);
    bool SubstituteImpl(IRMemReadOp *op);
    bool SubstituteImpl(IRMemWriteOp *op);
    bool SubstituteImpl(IRPreloadOp *op);
    bool SubstituteImpl(IRLogicalShiftLeftOp *op);
    bool SubstituteImpl(IRLogicalShiftRightOp *op);
    bool SubstituteImpl(IRArithmeticShiftRightOp *op);
    bool SubstituteImpl(IRRotateRightOp *op);
    bool SubstituteImpl(IRRotateRightExtendedOp *op);
    bool SubstituteImpl(IRBitwiseAndOp *op);
    bool SubstituteImpl(IRBitwiseOrOp *op);
    bool SubstituteImpl(IRBitwiseXorOp *op);
    bool SubstituteImpl(IRBitClearOp *op);
    bool SubstituteImpl(IRCountLeadingZerosOp *op);
    bool SubstituteImpl(IRAddOp *op);
    bool SubstituteImpl(IRAddCarryOp *op);
    bool SubstituteImpl(IRSubtractOp *op);
    bool SubstituteImpl(IRSubtractCarryOp *op);
    bool SubstituteImpl(IRMoveOp *op);
    bool SubstituteImpl(IRMoveNegatedOp *op);
    bool SubstituteImpl(IRSaturatingAddOp *op);
    bool SubstituteImpl(IRSaturatingSubtractOp *op);
    bool SubstituteImpl(IRMultiplyOp *op);
    bool SubstituteImpl(IRMultiplyLongOp *op);
    bool SubstituteImpl(IRAddLongOp *op);
    bool SubstituteImpl(IRStoreFlagsOp *op);
    bool SubstituteImpl(IRLoadFlagsOp *op);
    bool SubstituteImpl(IRLoadStickyOverflowOp *op);
    bool SubstituteImpl(IRBranchOp *op);
    bool SubstituteImpl(IRBranchExchangeOp *op);
    bool SubstituteImpl(IRStoreCopRegisterOp *op);
    bool SubstituteImpl(IRCopyVarOp *op);
};

} // namespace armajitto::ir
