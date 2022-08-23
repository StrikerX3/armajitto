#include "basic_peephole_opts.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

void BasicPeepholeOptimizerPass::Process(IRSetRegisterOp *op) {
    Substitute(op->src);
    ConsumeValue(op->src);
}

void BasicPeepholeOptimizerPass::Process(IRSetCPSROp *op) {
    Substitute(op->src);
    ConsumeValue(op->src);
}

void BasicPeepholeOptimizerPass::Process(IRSetSPSROp *op) {
    Substitute(op->src);
    ConsumeValue(op->src);
}

void BasicPeepholeOptimizerPass::Process(IRMemReadOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRMemWriteOp *op) {
    Substitute(op->src);
    Substitute(op->address);
    ConsumeValue(op->src);
    ConsumeValue(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRPreloadOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    Substitute(op->value);
    Substitute(op->amount);

    auto optimized = [this, op] {
        // Cannot optimize if the carry flag is affected
        if (op->setCarry) {
            return false;
        }

        // Requires the value to be a variable and the amount to be an immediate
        if (op->value.immediate || !op->amount.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // LSL shifts bits left, shifting in zeros
        value->LogicalShiftLeft(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
        ConsumeValue(op->amount);
    }
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);

    auto optimized = [this, op] {
        // Cannot optimize if the carry flag is affected
        if (op->setCarry) {
            return false;
        }

        // Requires the value to be a variable and the amount to be an immediate
        if (op->value.immediate || !op->amount.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // LSR shifts bits right, shifting in zeros
        value->LogicalShiftRight(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
        ConsumeValue(op->amount);
    }
}

void BasicPeepholeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);

    auto optimized = [this, op] {
        // Cannot optimize if the carry flag is affected
        if (op->setCarry) {
            return false;
        }

        // Requires the value to be a variable and the amount to be an immediate
        if (op->value.immediate || !op->amount.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // ASR shifts bits right, shifting in the most significant (sign) bit
        // Requires the sign bit to be known
        return value->ArithmeticShiftRight(op->amount.imm.value);
    }();

    if (!optimized) {
        ConsumeValue(op->value);
        ConsumeValue(op->amount);
    }
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);

    auto optimized = [this, op] {
        // Cannot optimize if the carry flag is affected
        if (op->setCarry) {
            return false;
        }

        // Requires the value to be a variable and the amount to be an immediate
        if (op->value.immediate || !op->amount.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // ROR rotates bits right
        value->RotateRight(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
        ConsumeValue(op->amount);
    }
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    Substitute(op->value);

    auto optimized = [this, op] {
        // Cannot optimize if the carry flag is affected
        if (op->setCarry) {
            return false;
        }

        // Requires the value to be a variable
        if (op->value.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // RRX rotates bits right by one, shifting in the carry flag
        // TODO: track carry flag
        // value->RotateRightExtended(carry);
        // return true;
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Requires a variable/immediate pair in lhs and rhs
        if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *pair;

            // Must derive from existing value
            auto *value = DeriveKnownBits(op->dst, var, op);
            if (value == nullptr) {
                return false;
            }

            // AND clears all zero bits
            value->Clear(~imm);
            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Requires a variable/immediate pair in lhs and rhs
        if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *pair;

            // Must derive from existing value
            auto *value = DeriveKnownBits(op->dst, var, op);
            if (value == nullptr) {
                return false;
            }

            // OR sets all one bits
            value->Set(imm);
            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Requires a variable/immediate pair in lhs and rhs
        if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *pair;

            // Must derive from existing value
            auto *value = DeriveKnownBits(op->dst, var, op);
            if (value == nullptr) {
                return false;
            }

            // XOR flips all one bits
            value->Flip(imm);
            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Requires a variable/immediate pair in lhs and rhs
        if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *pair;

            // Must derive from existing value
            auto *value = DeriveKnownBits(op->dst, var, op);
            if (value == nullptr) {
                return false;
            }

            // BIC clears all one bits
            value->Clear(imm);
            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
    }
}

void BasicPeepholeOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    ConsumeValue(op->value);
}

void BasicPeepholeOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // The value must be a variable
        if (op->value.immediate) {
            return false;
        }

        CopyVariable(op->dst, op->value.var, op);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
    }
}

void BasicPeepholeOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);

    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // The value must be a variable
        if (op->value.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveKnownBits(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // MVN inverts all bits
        value->Flip(~0);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
    }
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BasicPeepholeOptimizerPass::Process(IRAddLongOp *op) {
    Substitute(op->lhsLo);
    Substitute(op->lhsHi);
    Substitute(op->rhsLo);
    Substitute(op->rhsHi);
    ConsumeValue(op->lhsLo);
    ConsumeValue(op->lhsHi);
    ConsumeValue(op->rhsLo);
    ConsumeValue(op->rhsHi);
}

void BasicPeepholeOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->values);
    ConsumeValue(op->values);
}

void BasicPeepholeOptimizerPass::Process(IRLoadFlagsOp *op) {
    Substitute(op->srcCPSR);
    ConsumeValue(op->srcCPSR);
}

void BasicPeepholeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    ConsumeValue(op->srcCPSR);
}

void BasicPeepholeOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
    ConsumeValue(op->srcValue);
}

void BasicPeepholeOptimizerPass::Process(IRConstantOp *op) {
    AssignConstant(op->dst, op->value);
}

void BasicPeepholeOptimizerPass::Process(IRCopyVarOp *op) {
    Substitute(op->var);
    CopyVariable(op->dst, op->var, op);
}

// ---------------------------------------------------------------------------------------------------------------------

void BasicPeepholeOptimizerPass::ResizeValues(size_t index) {
    if (m_values.size() <= index) {
        m_values.resize(index + 1);
    }
}

void BasicPeepholeOptimizerPass::AssignConstant(VariableArg var, uint32_t value) {
    if (!var.var.IsPresent()) {
        return;
    }
    const auto index = var.var.Index();
    ResizeValues(index);
    auto &dstValue = m_values[index];
    dstValue.valid = true;
    dstValue.knownBitsMask = ~0;
    dstValue.knownBitsValue = value;
    dstValue.flippedBits = 0;
    dstValue.rotateOffset = 0;
}

void BasicPeepholeOptimizerPass::CopyVariable(VariableArg var, VariableArg src, IROp *op) {
    if (!var.var.IsPresent()) {
        return;
    }
    if (!src.var.IsPresent()) {
        return;
    }

    const auto srcIndex = src.var.Index();
    if (srcIndex >= m_values.size()) {
        return;
    }

    const auto dstIndex = var.var.Index();
    ResizeValues(dstIndex);
    auto &srcValue = m_values[srcIndex];
    auto &dstValue = m_values[dstIndex];
    dstValue = srcValue;
    dstValue.prev = src.var;
    dstValue.writerOp = op;
}

auto BasicPeepholeOptimizerPass::DeriveKnownBits(VariableArg var, VariableArg src, IROp *op) -> Value * {
    if (!var.var.IsPresent()) {
        return nullptr;
    }
    if (!src.var.IsPresent()) {
        return nullptr;
    }

    const auto srcIndex = src.var.Index();
    const auto dstIndex = var.var.Index();
    ResizeValues(dstIndex);

    auto &dstValue = m_values[dstIndex];
    auto &srcValue = m_values[srcIndex];
    dstValue.valid = true;
    dstValue.prev = src.var;
    dstValue.writerOp = op;
    if (srcIndex < m_values.size() && m_values[srcIndex].valid) {
        dstValue.source = srcValue.source;
        dstValue.knownBitsMask = srcValue.knownBitsMask;
        dstValue.knownBitsValue = srcValue.knownBitsValue;
        dstValue.flippedBits = srcValue.flippedBits;
        dstValue.rotateOffset = srcValue.rotateOffset;
    } else {
        dstValue.source = src.var;
    }
    return &dstValue;
}

auto BasicPeepholeOptimizerPass::GetValue(VariableArg var) -> Value * {
    if (!var.var.IsPresent()) {
        return nullptr;
    }

    const auto varIndex = var.var.Index();
    if (varIndex >= m_values.size()) {
        return nullptr;
    }

    auto &value = m_values[varIndex];
    if (value.valid) {
        return &value;
    }
    return nullptr;
}

void BasicPeepholeOptimizerPass::ConsumeValue(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    Value *value = GetValue(var);
    if (value == nullptr) {
        return;
    }
    if (!value->valid) {
        return;
    }

    bool match = false;
    if (value->knownBitsMask == ~0) {
        // The entire value is known

        // Check if the sequence of instructions contains exactly this instruction:
        //   const <var>, <value->value>
        if (value->prev == value->source) {
            if (auto maybeConstOp = Cast<IRConstantOp>(value->writerOp)) {
                auto *constOp = *maybeConstOp;
                match = (constOp->dst == var) && (constOp->value == value->knownBitsValue);
            }
        }

        // Replace the sequence if it doesn't match
        if (!match) {
            // Replace the last instruction with a const definition
            IROp *currPos = m_emitter.GetCurrentOp();
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite().Constant(var, value->knownBitsValue);
                m_emitter.GoTo(currPos);
            }
        }
    } else if (value->knownBitsMask != 0) {
        // Some of the bits are known
        const uint32_t ones = value->knownBitsValue & value->knownBitsMask;
        const uint32_t zeros = ~value->knownBitsValue & value->knownBitsMask;
        const uint32_t flips = value->flippedBits & ~value->knownBitsMask;
        const uint32_t rotate = value->rotateOffset;

        // Check if the sequence of instructions contains an ORR (if ones is non-zero), BIC (if zeros is non-zero)
        // and/or EOR (if flips is non-zero), and that the first consumed variable is value->source and the last output
        // variable is var.
        match = BitwiseOpsMatchState{*value, var.var, m_values}.Check(value);
        if (!match) {
            // Replace the last instruction with ROR for rotation, ORR for ones, BIC for zeros and XOR for flips
            IROp *currPos = m_emitter.GetCurrentOp();
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite();

                Variable result = value->source;

                // Emit a ROR for rotation
                if (rotate != 0) {
                    result = m_emitter.RotateRight(result, rotate, false);
                }

                // Emit an ORR for all known one bits
                if (ones != 0) {
                    result = m_emitter.BitwiseOr(result, ones, false);
                }

                // Emit a BIC for all known zero bits
                if (zeros != 0) {
                    result = m_emitter.BitClear(result, zeros, false);
                }

                // Emit a XOR for all unknown flipped bits
                if (flips != 0) {
                    result = m_emitter.BitwiseXor(result, flips, false);
                }
                Assign(var, result);
                var = result;

                m_emitter.GoTo(currPos);
            }
        }
    }

    // Erase previous instructions if changed
    if (!match) {
        value = GetValue(value->prev);
        while (value != nullptr) {
            if (value->writerOp != nullptr) {
                m_emitter.Erase(value->writerOp);
            }
            value = GetValue(value->prev);
        }
    }
}

void BasicPeepholeOptimizerPass::ConsumeValue(VarOrImmArg &var) {
    if (!var.immediate) {
        ConsumeValue(var.var);
    }
}

void BasicPeepholeOptimizerPass::ResizeVarSubsts(size_t index) {
    if (m_varSubsts.size() <= index) {
        m_varSubsts.resize(index + 1);
    }
}

void BasicPeepholeOptimizerPass::Assign(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto varIndex = dst.var.Index();
    ResizeVarSubsts(varIndex);
    m_varSubsts[varIndex] = src.var;
}

void BasicPeepholeOptimizerPass::Substitute(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    const auto varIndex = var.var.Index();
    if (varIndex < m_varSubsts.size() && m_varSubsts[varIndex].IsPresent()) {
        MarkDirty(var != m_varSubsts[varIndex]);
        var = m_varSubsts[varIndex];
    }
}

void BasicPeepholeOptimizerPass::Substitute(VarOrImmArg &var) {
    if (!var.immediate) {
        Substitute(var.var);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

BasicPeepholeOptimizerPass::BitwiseOpsMatchState::BitwiseOpsMatchState(Value &value, Variable expectedOutput,
                                                                       const std::vector<Value> &values)
    : ones(value.Ones())
    , zeros(value.Zeros())
    , flips(value.Flips())
    , rotate(value.RotateOffset())
    , expectedInput(value.source)
    , expectedOutput(expectedOutput)
    , values(values) {

    hasOnes = (ones == 0);
    hasZeros = (zeros == 0);
    hasFlips = (flips == 0);
    hasRotate = (rotate == 0);
}

bool BasicPeepholeOptimizerPass::BitwiseOpsMatchState::Check(const Value *value) {
    while (valid && value != nullptr) {
        VisitIROp(value->writerOp, *this);
        if (!value->prev.IsPresent()) {
            break;
        }
        const auto varIndex = value->prev.Index();
        if (varIndex >= values.size()) {
            break;
        }
        value = &values[varIndex];
        if (!value->valid) {
            break;
        }
    }
    return Valid();
}

bool BasicPeepholeOptimizerPass::BitwiseOpsMatchState::Valid() const {
    return valid && hasOnes && hasZeros && hasFlips && inputMatches && outputMatches;
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseOrOp *op) {
    CommonCheck(hasOnes, ones, op->lhs, op->rhs, op->dst);
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::operator()(IRBitClearOp *op) {
    CommonCheck(hasZeros, zeros, op->lhs, op->rhs, op->dst);
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseXorOp *op) {
    CommonCheck(hasFlips, flips, op->lhs, op->rhs, op->dst);
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::operator()(IRRotateRightOp *op) {
    if (!valid) {
        return;
    }

    if (!hasRotate) {
        // Found the instruction; check if the parameters match
        if (!op->value.immediate && op->amount.immediate) {
            hasRotate = (op->amount.imm.value == rotate);
            CheckInputVar(op->value.var.var);
            CheckOutputVar(op->dst.var);
        }
    } else {
        // Found more than once or matchValue == 0
        valid = false;
    }
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::CommonCheck(bool &flag, uint32_t matchValue, VarOrImmArg &lhs,
                                                                   VarOrImmArg &rhs, VariableArg dst) {
    if (!valid) {
        return;
    }

    if (!flag) {
        // Found the instruction; check if the parameters match
        if (auto split = SplitImmVarPair(lhs, rhs)) {
            auto [imm, var] = *split;
            flag = (imm == matchValue);
            CheckInputVar(var);
            CheckOutputVar(dst.var);
        }
    } else {
        // Found more than once or matchValue == 0
        valid = false;
    }
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::CheckInputVar(Variable var) {
    // Since we're checking in reverse order, this should be the last instruction in the sequence
    // Check only after all instructions have been matched
    if (hasOnes && hasZeros && hasFlips) {
        inputMatches = (var == expectedInput);
    }
}

void BasicPeepholeOptimizerPass::BitwiseOpsMatchState::CheckOutputVar(Variable var) {
    // Since we're checking in reverse order, this should be the first instruction in the sequence
    if (first) {
        outputMatches = (var == expectedOutput);
        first = false;
    }
}

} // namespace armajitto::ir
