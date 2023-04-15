#include "arithmetic_ops_coalescence.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

ArithmeticOpsCoalescenceOptimizerPass::ArithmeticOpsCoalescenceOptimizerPass(Emitter &emitter,
                                                                             std::pmr::memory_resource &alloc)
    : OptimizerPassBase(emitter)
    , m_values(&alloc)
    , m_sortedVars(&alloc)
    , m_reanalysisChain(&alloc)
    , m_varLifetimes(alloc)
    , m_varSubst(emitter.VariableCount(), alloc) {

    const uint32_t varCount = emitter.VariableCount();
    m_values.resize(varCount);

    m_varLifetimes.Analyze(emitter.GetBlock());
}

void ArithmeticOpsCoalescenceOptimizerPass::Reset() {
    std::fill(m_values.begin(), m_values.end(), Value{});
    m_varSubst.Reset();
    m_hostFlagsStateTracker.Reset();
    m_varLifetimes.Analyze(m_emitter.GetBlock());
}

void ArithmeticOpsCoalescenceOptimizerPass::PreProcess(IROp *op) {
    MarkDirty(m_varSubst.Substitute(op));
}

void ArithmeticOpsCoalescenceOptimizerPass::PostProcess(IROp *op) {
    m_hostFlagsStateTracker.Update(op);
}

// ---------------------------------------------------------------------------------------------------------------------

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetRegisterOp *op) {
    ConsumeValue(op, op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetCPSROp *op) {
    ConsumeValue(op, op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetSPSROp *op) {
    ConsumeValue(op, op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMemReadOp *op) {
    ConsumeValue(op, op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMemWriteOp *op) {
    ConsumeValues(op, op->src, op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRPreloadOp *op) {
    ConsumeValue(op, op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    ConsumeValues(op, op->value, op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    ConsumeValues(op, op->value, op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    ConsumeValues(op, op->value, op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRRotateRightOp *op) {
    ConsumeValues(op, op->value, op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    ConsumeValue(op, op->value);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitwiseAndOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitwiseOrOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitwiseXorOp *op) {
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

            // EOR with all bits set is equivalent to -x - 1
            if (imm == ~0) {
                value->FlipAllBits();

                // Coalesce previous operation into this if possible
                Coalesce(*value, op->dst.var, var, op);

                return true;
            }

            // Cannot optimize other cases
            return false;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    ConsumeValue(op, op->value);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRAddOp *op) {
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

            // ADD adds to the running sum
            value->Add(imm);

            // Coalesce previous operation into this if possible
            Coalesce(*value, op->dst.var, var, op);

            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRAddCarryOp *op) {
    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Cannot optimize if the host carry flag is unknown
        auto carryFlag = m_hostFlagsStateTracker.Carry();
        if (!carryFlag) {
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

            // Include the carry value
            if (*carryFlag) {
                ++imm;
            }

            // ADC adds to the running sum
            value->Add(imm);

            // Coalesce previous operation into this if possible
            Coalesce(*value, op->dst.var, var, op);

            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSubtractOp *op) {
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

            if (op->lhs.immediate) {
                // When the immediate is on the left-hand side, SUB adds to the running sum and negates the value
                value->Negate();
                value->Add(imm);
            } else {
                // When the immediate is on the right-hand side, SUB subtracts from the running sum
                value->Subtract(imm);
            }

            // Coalesce previous operation into this if possible
            Coalesce(*value, op->dst.var, var, op);

            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSubtractCarryOp *op) {
    auto optimized = [this, op] {
        // Cannot optimize if flags are affected
        if (op->flags != arm::Flags::None) {
            return false;
        }

        // Cannot optimize if the host carry flag is unknown
        auto carryFlag = m_hostFlagsStateTracker.Carry();
        if (!carryFlag) {
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

            if (op->lhs.immediate) {
                // Include the carry value
                if (!*carryFlag) {
                    --imm;
                }

                // When the immediate is on the left-hand side, SBC adds to the running sum and negates the value
                value->Negate();
                value->Add(imm);
            } else {
                // Include the carry value
                if (!*carryFlag) {
                    ++imm;
                }

                // When the immediate is on the right-hand side, SBC subtracts from the running sum
                value->Subtract(imm);
            }

            // Coalesce previous operation into this if possible
            Coalesce(*value, op->dst.var, var, op);

            return true;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValues(op, op->lhs, op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMoveOp *op) {
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

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMoveNegatedOp *op) {
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
        value->FlipAllBits();

        // Coalesce previous operation into this if possible
        Coalesce(*value, op->dst.var, op->value.var.var, op);

        return true;
    }();

    if (!optimized) {
        ConsumeValue(op, op->value);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    ConsumeValues(op, op->lhs, op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    ConsumeValues(op, op->lhsLo, op->lhsHi, op->rhsLo, op->rhsHi);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRStoreFlagsOp *op) {
    ConsumeValue(op, op->values);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLoadFlagsOp *op) {
    ConsumeValue(op, op->srcCPSR);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    ConsumeValue(op, op->srcCPSR);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBranchOp *op) {
    ConsumeValue(op, op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBranchExchangeOp *op) {
    ConsumeValue(op, op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    ConsumeValue(op, op->srcValue);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRCopyVarOp *op) {
    CopyValue(op->dst, op->var, op);
}

// ---------------------------------------------------------------------------------------------------------------------

void ArithmeticOpsCoalescenceOptimizerPass::ResizeValues(size_t index) {
    if (m_values.size() <= index) {
        m_values.resize(index + 1);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::CopyValue(VariableArg var, VariableArg src, IROp *op) {
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

auto ArithmeticOpsCoalescenceOptimizerPass::DeriveValue(VariableArg var, VariableArg src, IROp *op) -> Value * {
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
        dstValue.runningSum = srcValue.runningSum;
        dstValue.negated = srcValue.negated;
        srcValue.derived = true;
    } else {
        dstValue.source = src.var;
    }
    return &dstValue;
}

auto ArithmeticOpsCoalescenceOptimizerPass::GetValue(Variable var) -> Value * {
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

template <typename... Args>
void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValues(IROp *op, Args &...args) {
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

void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    ConsumeValue(op, var.var);
}

void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, VarOrImmArg &var) {
    if (!var.immediate) {
        ConsumeValue(op, var.var);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValue(IROp *op, Variable &var) {
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
    if (value->runningSum != 0 || value->negated) {
        // The value was changed

        // Emit ADD <dst>, <source>, <runningSum> when negated == false
        // Emit SUB <dst>, <runningSum>, <source> when negated == true
        // Emit MVN <dst>, <source> when negated == true and runningSum == -1

        // Check if the sequence of instructions contains exactly one of the instructions above
        if (value->prev == value->source) {
            if (value->negated) {
                if (auto subOp = Cast<IRSubtractOp>(value->writerOp)) {
                    const bool dstMatch = (subOp->dst == var);
                    const bool fwdMatch = (subOp->lhs == value->runningSum) && (subOp->rhs == value->source);
                    const bool revMatch = (subOp->lhs == value->source) && (subOp->rhs == value->runningSum);
                    const bool paramsMatch = (fwdMatch || revMatch);
                    const bool flagsMatch = (subOp->flags == arm::Flags::None);
                    match = dstMatch && paramsMatch && flagsMatch;
                } else if (auto mvnOp = Cast<IRMoveNegatedOp>(value->writerOp)) {
                    const bool dstMatch = (mvnOp->dst == var);
                    const bool srcMatch = (mvnOp->value == value->source);
                    const bool flagsMatch = (mvnOp->flags == arm::Flags::None);
                    match = dstMatch && srcMatch && flagsMatch;
                }
            } else {
                if (auto addOp = Cast<IRAddOp>(value->writerOp)) {
                    const bool dstMatch = (addOp->dst == var);
                    const bool fwdMatch = (addOp->lhs == value->runningSum) && (addOp->rhs == value->source);
                    const bool revMatch = (addOp->lhs == value->source) && (addOp->rhs == value->runningSum);
                    const bool paramsMatch = (fwdMatch || revMatch);
                    const bool flagsMatch = (addOp->flags == arm::Flags::None);
                    match = dstMatch && paramsMatch && flagsMatch;
                } else if (auto subOp = Cast<IRSubtractOp>(value->writerOp)) {
                    const bool dstMatch = (subOp->dst == var);
                    const bool fwdMatch = (subOp->lhs == -value->runningSum) && (subOp->rhs == value->source);
                    const bool revMatch = (subOp->lhs == value->source) && (subOp->rhs == -value->runningSum);
                    const bool paramsMatch = (fwdMatch || revMatch);
                    const bool flagsMatch = (subOp->flags == arm::Flags::None);
                    match = dstMatch && paramsMatch && flagsMatch;
                }
            }
        }

        // Replace the sequence if it doesn't match
        if (!match) {
            // Replace the last instruction with one of the arithmetic instructions
            if (value->writerOp != nullptr) {
                auto _ = m_emitter.GoTo(value->writerOp);
                OverwriteCoalescedOp(var, *value);
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

void ArithmeticOpsCoalescenceOptimizerPass::Coalesce(Value &value, Variable dst, Variable src, IROp *op) {
    if (value.valid && value.source != value.prev) {
        auto *prevValue = GetValue(value.prev);
        if (prevValue != nullptr && !prevValue->used && m_varLifetimes.IsEndOfLife(src, op)) {
            m_emitter.Erase(prevValue->writerOp);
            OverwriteCoalescedOp(dst, value);
        } else {
            prevValue->used = true;
        }
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::OverwriteCoalescedOp(Variable var, Value &value) {
    m_emitter.Overwrite();
    if (value.negated) {
        if (value.runningSum == -1) {
            m_emitter.MoveNegated(var, value.source, false);
        } else {
            m_emitter.Subtract(var, value.runningSum, value.source, false);
        }
    } else {
        m_emitter.Add(var, value.runningSum, value.source, false);
    }
}

} // namespace armajitto::ir
