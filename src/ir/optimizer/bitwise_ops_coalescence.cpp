#include "bitwise_ops_coalescence.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsCoalescenceOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter)
    , m_varSubst(emitter.VariableCount()) {

    const uint32_t varCount = emitter.VariableCount();
    m_values.resize(varCount);
}

void BitwiseOpsCoalescenceOptimizerPass::PreProcess(IROp *op) {
    MarkDirty(m_varSubst.Substitute(op));
}

void BitwiseOpsCoalescenceOptimizerPass::PostProcess(IROp *op) {
    m_hostFlagsStateTracker.Update(op);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetRegisterOp *op) {
    ConsumeValue(op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetCPSROp *op) {
    ConsumeValue(op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetSPSROp *op) {
    ConsumeValue(op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMemReadOp *op) {
    ConsumeValue(op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMemWriteOp *op) {
    ConsumeValue(op->src);
    ConsumeValue(op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRPreloadOp *op) {
    ConsumeValue(op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLogicalShiftRightOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRRotateRightOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRRotateRightExtendedOp *op) {
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

        // The host carry flag state must be known
        auto hostCarry = m_hostFlagsStateTracker.Carry();
        if (!hostCarry) {
            return false;
        }

        // RRX rotates bits right by one, shifting in the carry flag
        value->RotateRightExtended(*hostCarry);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBitwiseAndOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBitwiseOrOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBitwiseXorOp *op) {
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

            // EOR flips all one bits
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    ConsumeValue(op->value);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddCarryOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSubtractOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSubtractCarryOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMoveOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMoveNegatedOp *op) {
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

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    ConsumeValue(op->lhsLo);
    ConsumeValue(op->lhsHi);
    ConsumeValue(op->rhsLo);
    ConsumeValue(op->rhsHi);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRStoreFlagsOp *op) {
    ConsumeValue(op->values);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLoadFlagsOp *op) {
    ConsumeValue(op->srcCPSR);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    ConsumeValue(op->srcCPSR);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBranchOp *op) {
    ConsumeValue(op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBranchExchangeOp *op) {
    ConsumeValue(op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    ConsumeValue(op->srcValue);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRConstantOp *op) {
    AssignConstant(op->dst, op->value);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRCopyVarOp *op) {
    CopyVariable(op->dst, op->var, op);
}

// ---------------------------------------------------------------------------------------------------------------------

void BitwiseOpsCoalescenceOptimizerPass::ResizeValues(size_t index) {
    if (m_values.size() <= index) {
        m_values.resize(index + 1);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::AssignConstant(VariableArg var, uint32_t value) {
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

void BitwiseOpsCoalescenceOptimizerPass::CopyVariable(VariableArg var, VariableArg src, IROp *op) {
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

auto BitwiseOpsCoalescenceOptimizerPass::DeriveKnownBits(VariableArg var, VariableArg src, IROp *op) -> Value * {
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
    dstValue.valid = true;
    dstValue.prev = src.var;
    dstValue.writerOp = op;
    if (srcIndex < m_values.size() && m_values[srcIndex].valid) {
        auto &srcValue = m_values[srcIndex];
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

auto BitwiseOpsCoalescenceOptimizerPass::GetValue(VariableArg var) -> Value * {
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

void BitwiseOpsCoalescenceOptimizerPass::ConsumeValue(VariableArg &var) {
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
            // Replace the last instruction with ROR for rotation, ORR for ones, BIC for zeros and EOR for flips
            IROp *currPos = m_emitter.GetCurrentOp();
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite();

                Variable result = value->source;

                // Emit a ROR or LSR for rotation
                if (rotate != 0) {
                    const uint32_t rotateMask = ~(~0 >> rotate);
                    if ((value->knownBitsMask & rotateMask) == rotateMask) {
                        // Emit LSR when all <rotate> most significant bits are known
                        result = m_emitter.LogicalShiftRight(result, rotate, false);
                    } else {
                        // Emit ROR otherwise
                        result = m_emitter.RotateRight(result, rotate, false);
                    }
                }

                if (ones != 0 && zeros != 0 && flips != 0) {
                    // Emit an optimized sequence with BIC/EOR instead of ORR/BIC/EOR by merging the ones into the other
                    // two instructions. This works because BIC will clear all one bits to zeros, then EOR will flip
                    // those to one.
                    result = m_emitter.BitClear(result, zeros | ones, false);
                    result = m_emitter.BitwiseXor(result, flips | ones, false);
                } else {
                    // Emit ORR for all known one bits
                    if (ones != 0) {
                        result = m_emitter.BitwiseOr(result, ones, false);
                    }

                    // Emit BIC for all known zero bits
                    if (zeros != 0) {
                        result = m_emitter.BitClear(result, zeros, false);
                    }

                    // Emit EOR for all unknown flipped bits
                    if (flips != 0) {
                        result = m_emitter.BitwiseXor(result, flips, false);
                    }
                }
                m_varSubst.Assign(var, result);
                var = result;

                m_emitter.GoTo(currPos);
            }
        }
    } else {
        // Erase the whole sequence of instructions since they don't change anything
        Variable result = value->source;
        m_varSubst.Assign(var, result);
        var = result;
        m_emitter.Erase(value->writerOp);
    }

    // Erase previous instructions if changed
    if (!match) {
        value = GetValue(value->prev);
        while (value != nullptr) {
            m_emitter.Erase(value->writerOp);
            value = GetValue(value->prev);
        }
    }
}

void BitwiseOpsCoalescenceOptimizerPass::ConsumeValue(VarOrImmArg &var) {
    if (!var.immediate) {
        ConsumeValue(var.var);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::BitwiseOpsMatchState(Value &value, Variable expectedOutput,
                                                                               const std::vector<Value> &values)
    : ones(value.Ones())
    , zeros(value.Zeros())
    , flips(value.Flips())
    , rotate(value.RotateOffset())
    , expectedInput(value.source)
    , expectedOutput(expectedOutput)
    , values(values) {

    // When we have the trifecta, only look for BIC and EOR
    trifecta = (ones != 0) && (zeros != 0) && (flips != 0);

    hasOnes = (ones == 0);
    hasZeros = (zeros == 0);
    hasFlips = (flips == 0);
    hasRotate = (rotate == 0);
}

bool BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::Check(const Value *value) {
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

bool BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::Valid() const {
    if (trifecta) {
        return valid && hasTrifectaClear && hasTrifectaFlip && inputMatches && outputMatches;
    } else {
        return valid && hasOnes && hasZeros && hasFlips && inputMatches && outputMatches;
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRLogicalShiftRightOp *op) {
    CommonShiftCheck(op->value, op->amount, op->dst);
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRRotateRightOp *op) {
    CommonShiftCheck(op->value, op->amount, op->dst);
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseOrOp *op) {
    if (trifecta) {
        valid = false;
    } else {
        CommonCheck(hasOnes, ones, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitClearOp *op) {
    if (trifecta) {
        CommonCheck(hasTrifectaClear, zeros | ones, op->lhs, op->rhs, op->dst);
    } else {
        CommonCheck(hasZeros, zeros, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseXorOp *op) {
    if (trifecta) {
        CommonCheck(hasTrifectaFlip, flips | ones, op->lhs, op->rhs, op->dst);
    } else {
        CommonCheck(hasFlips, flips, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::CommonShiftCheck(VarOrImmArg &value, VarOrImmArg &amount,
                                                                                VariableArg dst) {
    if (!valid) {
        return;
    }

    if (!hasRotate) {
        // Found the instruction; check if the parameters match
        if (!value.immediate && amount.immediate) {
            hasRotate = (amount.imm.value == rotate);
            CheckInputVar(value.var.var);
            CheckOutputVar(dst.var);
        }
    } else {
        // Found more than once or matchValue == 0
        valid = false;
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::CommonCheck(bool &flag, uint32_t matchValue,
                                                                           VarOrImmArg &lhs, VarOrImmArg &rhs,
                                                                           VariableArg dst) {
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

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::CheckInputVar(Variable var) {
    // Since we're checking in reverse order, this should be the last instruction in the sequence
    // Check only after all instructions have been matched
    bool test;
    if (trifecta) {
        test = hasTrifectaClear && hasTrifectaFlip;
    } else {
        test = hasOnes && hasZeros && hasFlips;
    }
    if (test) {
        inputMatches = (var == expectedInput);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::CheckOutputVar(Variable var) {
    // Since we're checking in reverse order, this should be the first instruction in the sequence
    if (first) {
        outputMatches = (var == expectedOutput);
        first = false;
    }
}

} // namespace armajitto::ir
