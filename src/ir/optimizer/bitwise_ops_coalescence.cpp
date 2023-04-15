#include "bitwise_ops_coalescence.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsCoalescenceOptimizerPass(Emitter &emitter,
                                                                       std::pmr::memory_resource &alloc)
    : OptimizerPassBase(emitter)
    , m_values(&alloc)
    , m_sortedVars(&alloc)
    , m_reanalysisChain(&alloc)
    , m_varLifetimes(alloc)
    , m_varSubst(emitter.VariableCount(), alloc) {

    const uint32_t varCount = emitter.VariableCount();
    m_values.resize(varCount);
    m_varLifetimes.Analyze(m_emitter.GetBlock());
}

void BitwiseOpsCoalescenceOptimizerPass::Reset() {
    std::fill(m_values.begin(), m_values.end(), Value{});
    m_varSubst.Reset();
    m_hostFlagsStateTracker.Reset();
    m_varLifetimes.Analyze(m_emitter.GetBlock());
}

void BitwiseOpsCoalescenceOptimizerPass::PreProcess(IROp *op) {
    MarkDirty(m_varSubst.Substitute(op));
    m_varLifetimes.Update(op);
}

void BitwiseOpsCoalescenceOptimizerPass::PostProcess(IROp *op) {
    m_hostFlagsStateTracker.Update(op);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetRegisterOp *op) {
    ConsumeValue(op, op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetCPSROp *op) {
    ConsumeValue(op, op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSetSPSROp *op) {
    ConsumeValue(op, op->src);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMemReadOp *op) {
    ConsumeValue(op, op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMemWriteOp *op) {
    ConsumeValues(op, op->src, op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRPreloadOp *op) {
    ConsumeValue(op, op->address);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // LSL shifts bits left, shifting in zeros
        value->LogicalShiftLeft(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValues(op, op->value, op->amount);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // LSR shifts bits right, shifting in zeros
        value->LogicalShiftRight(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValues(op, op->value, op->amount);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // ASR shifts bits right, shifting in the most significant (sign) bit
        // Requires the sign bit to be known
        return value->ArithmeticShiftRight(op->amount.imm.value);
    }();

    if (!optimized) {
        ConsumeValues(op, op->value, op->amount);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // ROR rotates bits right
        value->RotateRight(op->amount.imm.value);
        return true;
    }();

    if (!optimized) {
        ConsumeValues(op, op->value, op->amount);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
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
        ConsumeValue(op, op->value);
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
            auto *value = DeriveValue(op->dst, var, op);
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
        ConsumeValues(op, op->lhs, op->rhs);
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
            auto *value = DeriveValue(op->dst, var, op);
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
        ConsumeValues(op, op->lhs, op->rhs);
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
            auto *value = DeriveValue(op->dst, var, op);
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
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Requires lhs to be a variable and rhs to be an immediate
        if (op->lhs.immediate || !op->rhs.immediate) {
            return false;
        }

        // Must derive from existing value
        auto *value = DeriveValue(op->dst, op->lhs.var.var, op);
        if (value == nullptr) {
            return false;
        }

        // BIC clears all one bits
        value->Clear(op->rhs.imm.value);

        return true;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    ConsumeValue(op, op->value);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddCarryOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSubtractOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSubtractCarryOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
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

        // MOV simply copies the value
        CopyValue(op->dst, op->value.var, op);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op, op->value);
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
        auto *value = DeriveValue(op->dst, op->value.var, op);
        if (value == nullptr) {
            return false;
        }

        // MVN inverts all bits
        value->Flip(~0);
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op, op->value);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    ConsumeValues(op, op->lhsLo, op->lhsHi, op->rhsLo, op->rhsHi);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRStoreFlagsOp *op) {
    ConsumeValue(op, op->values);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLoadFlagsOp *op) {
    ConsumeValue(op, op->srcCPSR);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    ConsumeValue(op, op->srcCPSR);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBranchOp *op) {
    ConsumeValue(op, op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRBranchExchangeOp *op) {
    ConsumeValue(op, op->address);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    ConsumeValue(op, op->srcValue);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRConstantOp *op) {
    AssignConstant(op->dst, op->value);
}

void BitwiseOpsCoalescenceOptimizerPass::Process(IRCopyVarOp *op) {
    CopyValue(op->dst, op->var, op);
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

void BitwiseOpsCoalescenceOptimizerPass::CopyValue(VariableArg var, VariableArg src, IROp *op) {
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

auto BitwiseOpsCoalescenceOptimizerPass::DeriveValue(VariableArg var, VariableArg src, IROp *op) -> Value * {
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
    dstValue.valid = false; // Not yet valid
    dstValue.prev = src.var;
    dstValue.writerOp = op;
    if (srcIndex < m_values.size() && m_values[srcIndex].valid && !m_values[srcIndex].consumed) {
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

auto BitwiseOpsCoalescenceOptimizerPass::GetValue(Variable var) -> Value * {
    const auto varIndex = var.Index();
    if (varIndex >= m_values.size()) {
        return nullptr;
    }

    auto &value = m_values[varIndex];
    if (value.valid) {
        return &value;
    }
    return nullptr;
}

static std::pair<bool, bool> ShiftMatch(uint32_t knownBitsMask, uint32_t knownBitsValue, uint32_t rotate,
                                        uint32_t shiftMask) {
    const bool basicMatch = (rotate != 0) && (knownBitsValue & shiftMask) == 0;
    const bool bitMatch = basicMatch && (knownBitsMask & shiftMask) == shiftMask;
    const bool exactMatch = basicMatch && (knownBitsMask == shiftMask);
    return {bitMatch, exactMatch};
};

template <typename... Args>
void BitwiseOpsCoalescenceOptimizerPass::ConsumeValues(IROp *op, Args &...args) {
    m_sortedVars.clear();
    (
        [&] {
            using T = std::decay_t<decltype(args)>;
            if constexpr (std::is_same_v<VariableArg, T>) {
                if (args.var.IsPresent()) {
                    m_sortedVars.push_back(&args.var);
                }
            } else if constexpr (std::is_same_v<VarOrImmArg, T>) {
                if (!args.immediate && args.var.var.IsPresent()) {
                    m_sortedVars.push_back(&args.var.var);
                }
            }
        }(),
        ...);
    std::sort(m_sortedVars.begin(), m_sortedVars.end(),
              [](const Variable *lhs, const Variable *rhs) { return lhs->Index() < rhs->Index(); });
    for (auto *var : m_sortedVars) {
        ConsumeValue(op, *var);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, VariableArg &arg) {
    if (arg.var.IsPresent()) {
        ConsumeValue(op, arg.var);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, VarOrImmArg &arg) {
    if (!arg.immediate) {
        ConsumeValue(op, arg.var);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, Variable &var) {
    Value *value = GetValue(var);
    if (value == nullptr) {
        return;
    }
    if (!value->valid) {
        return;
    }

    // Mark this value as consumed
    value->consumed = true;

    // Reanalyze the value in a previous value in the chain was consumed
    if (value->prev != value->source) {
        m_reanalysisChain.push_back(value->writerOp);
        auto var = value->prev;
        auto *prevValue = value;
        auto *chainValue = GetValue(value->prev);
        while (chainValue != nullptr && chainValue->valid) {
            if (chainValue->consumed) {
                // Found a consumed value; reanalyze from the next instruction
                prevValue->Reset();
                while (!m_reanalysisChain.empty()) {
                    IROp *op = m_reanalysisChain.back();
                    m_reanalysisChain.pop_back();
                    m_varSubst.Substitute(op);
                    VisitIROp(op, [this](auto op) -> void { Process(op); });
                }
                break;
            } else {
                m_reanalysisChain.push_back(chainValue->writerOp);
            }
            prevValue = chainValue;
            var = chainValue->prev;
            chainValue = GetValue(chainValue->prev);
        }
    }
    m_reanalysisChain.clear();

    bool match = false;
    if (value->knownBitsMask == ~0) {
        // The entire value is known

        // Check if the sequence of instructions contains exactly this instruction:
        //   const <var>, <value->value>
        if (value->prev == value->source) {
            if (auto constOp = Cast<IRConstantOp>(value->writerOp)) {
                match = (constOp->dst == var) && (constOp->value == value->knownBitsValue);
            }
        }

        // Replace the sequence if it doesn't match
        if (!match) {
            // Replace the last instruction with a const definition
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                auto _ = m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite().Constant(var, value->knownBitsValue);
            }
        }
    } else if (value->knownBitsMask != 0 || value->flippedBits != 0) {
        // Some of the bits are known
        const uint32_t ones = value->knownBitsValue & value->knownBitsMask;
        const uint32_t zeros = ~value->knownBitsValue & value->knownBitsMask;
        const uint32_t flips = value->flippedBits & ~value->knownBitsMask;
        const uint32_t rotate = value->rotateOffset;

        // Check if the sequence of instructions contains an ORR (if ones is non-zero), AND (if zeros is non-zero)
        // and/or EOR (if flips is non-zero), and that the first consumed variable is value->source and the last output
        // variable is var.
        match = BitwiseOpsMatchState{*value, var, m_values}.Check(value);
        if (!match) {
            // Replace the last instruction with:
            // - ROR, LSR or LSL for rotation or shifts
            // - ORR for ones
            // - AND for zeros (negated)
            // - EOR for flips
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                auto _ = m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite();

                Variable result = value->source;

                // Emit a ROR, LSR or LSL for rotation
                const uint32_t rightShiftMask = ~(~0u >> rotate);
                const uint32_t leftShiftMask = (rotate == 0) ? ~0 : ~(~0u << (32 - rotate));
                const auto [rightShiftBitMatch, rightShiftExactMatch] =
                    ShiftMatch(value->knownBitsMask, value->knownBitsValue, rotate, rightShiftMask);
                const auto [leftShiftBitMatch, leftShiftExactMatch] =
                    ShiftMatch(value->knownBitsMask, value->knownBitsValue, rotate, leftShiftMask);
                if (rotate != 0) {
                    if (rightShiftBitMatch) {
                        // Emit LSR when all <rotate> most significant bits are known to be zero
                        result = m_emitter.LogicalShiftRight(result, rotate, false);
                    } else if (leftShiftBitMatch) {
                        // Emit LSL when all <32 - rotate> least significant bits are known to be zero
                        result = m_emitter.LogicalShiftLeft(result, 32 - rotate, false);
                    } else {
                        // Emit ROR otherwise
                        result = m_emitter.RotateRight(result, rotate, false);
                    }
                }

                if (ones != 0 && zeros != 0 && flips != 0) {
                    // Emit an optimized sequence with AND/EOR instead of ORR/AND/EOR by merging the ones into the other
                    // two instructions. This works because AND will clear all negated one bits to zeros, then EOR will
                    // flip those to one.
                    result = m_emitter.BitwiseAnd(result, ~(zeros | ones), false);
                    result = m_emitter.BitwiseXor(result, flips | ones, false);
                } else {
                    // Emit ORR for all known one bits
                    if (ones != 0) {
                        result = m_emitter.BitwiseOr(result, ones, false);
                    }

                    // Emit AND for all known zero bits (negated), unless all of those bits are covered by LSR or LSL
                    if (zeros != 0 && !rightShiftExactMatch && !leftShiftExactMatch) {
                        result = m_emitter.BitwiseAnd(result, ~zeros, false);
                    }

                    if (flips == ~0) {
                        // Emit MVN if all bits are flipped
                        result = m_emitter.MoveNegated(result, false);
                    } else if (flips != 0) {
                        // Emit EOR for all unknown flipped bits
                        result = m_emitter.BitwiseXor(result, flips, false);
                    }
                }
                m_varSubst.Assign(var, result);
                var = result;
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
        auto var = value->prev;
        value = GetValue(value->prev);
        while (value != nullptr && value->valid && !value->consumed && m_varLifetimes.IsExpired(var)) {
            m_emitter.Erase(value->writerOp);
            var = value->prev;
            value = GetValue(value->prev);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::BitwiseOpsMatchState(Value &value, Variable expectedOutput,
                                                                               const std::pmr::vector<Value> &values)
    : ones(value.Ones())
    , zeros(value.Zeros())
    , flips(value.Flips())
    , rotate(value.RotateOffset())
    , expectedInput(value.source)
    , expectedOutput(expectedOutput)
    , values(values) {

    // When we have the trifecta, only look for AND and EOR
    trifecta = (ones != 0) && (zeros != 0) && (flips != 0);

    // When LSR is used and the only zero bits are the most significant <rotate> bits, we can omit AND.
    // This happens when all zeros are covered by the rotation mask and no other zeros exist.
    const uint32_t rightShiftMask = ~(~0u >> rotate);
    const uint32_t leftShiftMask = ~(~0u << (32 - rotate));
    const auto [rightShiftBitMatch, rightShiftExactMatch] =
        ShiftMatch(value.knownBitsMask, value.knownBitsValue, rotate, rightShiftMask);
    const auto [leftShiftBitMatch, leftShiftExactMatch] =
        ShiftMatch(value.knownBitsMask, value.knownBitsValue, rotate, leftShiftMask);
    hasOnes = (ones == 0);
    hasZeros = (zeros == 0) || leftShiftExactMatch || rightShiftExactMatch;
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
        if (!value->valid || value->consumed) {
            break;
        }
    }
    return Valid();
}

bool BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::Valid() const {
    if (trifecta) {
        return valid && hasTrifectaAnd && hasTrifectaFlip && inputMatches && outputMatches;
    } else {
        return valid && hasOnes && hasZeros && hasFlips && inputMatches && outputMatches;
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRLogicalShiftLeftOp *op) {
    CommonShiftCheck(op->value, op->amount, op->dst);
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRLogicalShiftRightOp *op) {
    CommonShiftCheck(op->value, op->amount, op->dst);
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRRotateRightOp *op) {
    CommonShiftCheck(op->value, op->amount, op->dst);
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseAndOp *op) {
    if (trifecta) {
        CommonCheck(hasTrifectaAnd, ~(zeros | ones), op->lhs, op->rhs, op->dst);
    } else {
        CommonCheck(hasZeros, ~zeros, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseOrOp *op) {
    if (trifecta) {
        valid = false;
    } else {
        CommonCheck(hasOnes, ones, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRBitwiseXorOp *op) {
    if (trifecta) {
        CommonCheck(hasTrifectaFlip, flips | ones, op->lhs, op->rhs, op->dst);
    } else {
        CommonCheck(hasFlips, flips, op->lhs, op->rhs, op->dst);
    }
}

void BitwiseOpsCoalescenceOptimizerPass::BitwiseOpsMatchState::operator()(IRMoveNegatedOp *op) {
    if (!valid) {
        return;
    }

    if (!hasFlips && zeros == 0 && ones == 0 && flips == ~0) {
        // Found the instruction; check if the parameters match
        if (!op->value.immediate) {
            hasFlips = true;
            CheckInputVar(op->value.var.var);
            CheckOutputVar(op->dst.var);
        }
    } else {
        // Found more than once or not in a valid sequence
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
        // Found more than once
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
        test = hasTrifectaAnd && hasTrifectaFlip;
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
