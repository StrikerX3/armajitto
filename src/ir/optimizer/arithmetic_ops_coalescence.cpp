#include "arithmetic_ops_coalescence.hpp"

namespace armajitto::ir {

ArithmeticOpsCoalescenceOptimizerPass::ArithmeticOpsCoalescenceOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter)
    , m_varSubst(emitter.VariableCount()) {

    const uint32_t varCount = emitter.VariableCount();
    m_values.resize(varCount);
}

void ArithmeticOpsCoalescenceOptimizerPass::PreProcess(IROp *op) {
    MarkDirty(m_varSubst.Substitute(op));
}

void ArithmeticOpsCoalescenceOptimizerPass::PostProcess(IROp *op) {
    m_hostFlagsStateTracker.Update(op);
}

// ---------------------------------------------------------------------------------------------------------------------

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetRegisterOp *op) {
    ConsumeValue(op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetCPSROp *op) {
    ConsumeValue(op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSetSPSROp *op) {
    ConsumeValue(op->src);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMemReadOp *op) {
    ConsumeValue(op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMemWriteOp *op) {
    ConsumeValue(op->src);
    ConsumeValue(op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRPreloadOp *op) {
    ConsumeValue(op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRRotateRightOp *op) {
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    ConsumeValue(op->value);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitwiseAndOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitwiseOrOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
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
                return true;
            }

            // Cannot optimize other cases
            return false;
        }

        // Not a variable/immediate pair
        return false;
    }();

    if (!optimized) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    ConsumeValue(op->value);
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
        ConsumeValue(op->value);
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
        return true;
    }();

    if (!optimized) {
        ConsumeValue(op->value);
    }
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    ConsumeValue(op->lhsLo);
    ConsumeValue(op->lhsHi);
    ConsumeValue(op->rhsLo);
    ConsumeValue(op->rhsHi);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRStoreFlagsOp *op) {
    ConsumeValue(op->values);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLoadFlagsOp *op) {
    ConsumeValue(op->srcCPSR);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    ConsumeValue(op->srcCPSR);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBranchOp *op) {
    ConsumeValue(op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRBranchExchangeOp *op) {
    ConsumeValue(op->address);
}

void ArithmeticOpsCoalescenceOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    ConsumeValue(op->srcValue);
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
    dstValue.valid = true;
    dstValue.prev = src.var;
    dstValue.writerOp = op;
    if (srcIndex < m_values.size() && m_values[srcIndex].valid) {
        auto &srcValue = m_values[srcIndex];
        dstValue.source = srcValue.source;
        dstValue.runningSum = srcValue.runningSum;
        dstValue.negated = srcValue.negated;
    } else {
        dstValue.source = src.var;
    }
    return &dstValue;
}

auto ArithmeticOpsCoalescenceOptimizerPass::GetValue(VariableArg var) -> Value * {
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

void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValue(VariableArg &var) {
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
    if (value->runningSum != 0 || value->negated) {
        // The value was changed

        // Emit ADD <dst>, <runningSum>, <source>  when negated == false
        // Emit SUB <dst>, <runningSum>, <source> when negated == true
        // Emit MVN <dst>, <source> when negated == true and runningSum == -1

        // Check if the sequence of instructions contains exactly one of the instructions above
        if (value->prev == value->source) {
            if (value->negated) {
                if (auto maybeSubOp = Cast<IRSubtractOp>(value->writerOp)) {
                    auto *subOp = *maybeSubOp;
                    const bool dstMatch = (subOp->dst == var);
                    const bool fwdMatch = (subOp->lhs == value->runningSum) && (subOp->rhs == value->source);
                    const bool revMatch = (subOp->lhs == value->source) && (subOp->rhs == value->runningSum);
                    const bool paramsMatch = (fwdMatch || revMatch);
                    const bool flagsMatch = (subOp->flags == arm::Flags::None);
                    match = dstMatch && paramsMatch && flagsMatch;
                }
                if (auto maybeMvnOp = Cast<IRMoveNegatedOp>(value->writerOp)) {
                    auto *mvnOp = *maybeMvnOp;
                    const bool dstMatch = (mvnOp->dst == var);
                    const bool srcMatch = (mvnOp->value == value->source);
                    const bool flagsMatch = (mvnOp->flags == arm::Flags::None);
                    match = dstMatch && srcMatch && flagsMatch;
                }
            } else {
                if (auto maybeAddOp = Cast<IRAddOp>(value->writerOp)) {
                    auto *addOp = *maybeAddOp;
                    const bool dstMatch = (addOp->dst == var);
                    const bool fwdMatch = (addOp->lhs == value->runningSum) && (addOp->rhs == value->source);
                    const bool revMatch = (addOp->lhs == value->source) && (addOp->rhs == value->runningSum);
                    const bool paramsMatch = (fwdMatch || revMatch);
                    const bool flagsMatch = (addOp->flags == arm::Flags::None);
                    match = dstMatch && paramsMatch && flagsMatch;
                }
            }
        }

        // Replace the sequence if it doesn't match
        if (!match) {
            // Replace the last instruction with one of the arithmetic instructions
            if (value->writerOp != nullptr) {
                auto _ = m_emitter.GoTo(value->writerOp);
                if (value->negated) {
                    if (value->runningSum == -1) {
                        m_emitter.Overwrite().MoveNegated(var, value->source, false);
                    } else {
                        m_emitter.Overwrite().Subtract(var, value->runningSum, value->source, false);
                    }
                } else {
                    m_emitter.Overwrite().Add(var, value->runningSum, value->source, false);
                }
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

void ArithmeticOpsCoalescenceOptimizerPass::ConsumeValue(VarOrImmArg &var) {
    if (!var.immediate) {
        ConsumeValue(var.var);
    }
}

} // namespace armajitto::ir
