#include "basic_peephole_opts.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cstdio>
#include <format>

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
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    ConsumeValue(op->value);
    ConsumeValue(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    ConsumeValue(op->value);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
        return;
    }

    // AND clears all zero bits
    if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *pair;
        DeriveKnownBits(op->dst, var, ~imm, 0, op);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
        return;
    }

    // OR sets all one bits
    if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *pair;
        DeriveKnownBits(op->dst, var, imm, imm, op);
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
        return;
    }

    // XOR flips all one bits
    if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *pair;
        if (auto optValue = GetValue(var)) {
            auto &value = *optValue;
            DeriveKnownBits(op->dst, var, value.knownBits & imm, value.value ^ imm, imm, op);
        }
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->lhs);
        ConsumeValue(op->rhs);
        return;
    }

    // BIC clears all one bits
    if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *pair;
        DeriveKnownBits(op->dst, var, imm, 0, op);
    }
}

void BasicPeepholeOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    ConsumeValue(op->value);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->value);
        return;
    }

    if (!op->value.immediate) {
        CopyVariable(op->dst, op->value.var, op);
    }
}

void BasicPeepholeOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
        ConsumeValue(op->value);
        return;
    }

    // MVN inverts all bits
    if (!op->value.immediate) {
        if (auto optValue = GetValue(op->value.var)) {
            auto &value = *optValue;
            DeriveKnownBits(op->dst, op->value.var, value.knownBits, ~value.value, ~0, op);
        }
    }
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    ConsumeValue(op->lhs);
    ConsumeValue(op->rhs);
    // TODO: implement
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
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->values);
    ConsumeValue(op->values);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLoadFlagsOp *op) {
    Substitute(op->srcCPSR);
    ConsumeValue(op->srcCPSR);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    ConsumeValue(op->srcCPSR);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
    ConsumeValue(op->address);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
    ConsumeValue(op->srcValue);
    // TODO: implement
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
    dstValue.knownBits = ~0;
    dstValue.value = value;
    dstValue.flippedBits = 0;

    auto __varStr = var.ToString();
    printf("assigned known bits: %s value=0x%08x -> known mask=0x%08x value=0x%08x flip=0x%08x\n", __varStr.c_str(),
           value, dstValue.knownBits, dstValue.value, dstValue.flippedBits);
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

    auto __varStr = var.ToString();
    auto __srcStr = src.ToString();
    printf("copied known bits: %s = %s -> known mask=0x%08x value=0x%08x flip=0x%08x\n", __varStr.c_str(),
           __srcStr.c_str(), dstValue.knownBits, dstValue.value, dstValue.flippedBits);
}

void BasicPeepholeOptimizerPass::DeriveKnownBits(VariableArg var, VariableArg src, uint32_t mask, uint32_t value,
                                                 IROp *op) {
    DeriveKnownBits(var, src, mask, value, 0, op);
}

void BasicPeepholeOptimizerPass::DeriveKnownBits(VariableArg var, VariableArg src, uint32_t mask, uint32_t value,
                                                 uint32_t flipped, IROp *op) {
    if (!var.var.IsPresent()) {
        return;
    }
    if (!src.var.IsPresent()) {
        return;
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
        dstValue.knownBits = srcValue.knownBits | mask;
        dstValue.value = (srcValue.value & ~mask) | (value & mask);
        dstValue.flippedBits = (srcValue.flippedBits ^ flipped) & ~mask;
    } else {
        dstValue.source = src.var;
        dstValue.knownBits = mask;
        dstValue.value = value & mask;
        dstValue.flippedBits = flipped & ~mask;
    }

    auto __varStr = var.ToString();
    auto __srcStr = src.ToString();
    printf("derived known bits: %s = %s -- mask=0x%08x value=0x%08x -> known mask=0x%08x value=0x%08x flips=0x%08x\n",
           __varStr.c_str(), __srcStr.c_str(), mask, value, dstValue.knownBits, dstValue.value, dstValue.flippedBits);
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

    auto __varStr = var.ToString();
    auto __srcStr = value->source.ToString();
    printf("consuming %s -> mask=0x%08x value=0x%08x src=%s\n", __varStr.c_str(), value->knownBits, value->value,
           __srcStr.c_str());

    bool match = false;
    if (value->knownBits == ~0) {
        // The entire value is known
        printf("  full value known!\n");

        // Check if the sequence of instructions contains exactly this instruction:
        //   const <var>, <value->value>
        if (value->prev == value->source) {
            if (auto maybeConstOp = Cast<IRConstantOp>(value->writerOp)) {
                auto *constOp = *maybeConstOp;
                match = (constOp->dst == var) && (constOp->value == value->value);
            }
        }

        // Replace the sequence if it doesn't match
        if (!match) {
            // Replace the last instruction with a const definition
            IROp *currPos = m_emitter.GetCurrentOp();
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                auto __writerOpStr = value->writerOp->ToString();
                m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite().Constant(var, value->value);
                m_emitter.GoTo(currPos);
                auto __newOpStr = m_emitter.GetCurrentOp()->Prev()->ToString();
                printf("    replaced '%s' with '%s'\n", __writerOpStr.c_str(), __newOpStr.c_str());
            }
        }
    } else if (value->knownBits != 0) {
        // Some of the bits are known
        const uint32_t ones = value->value & value->knownBits;
        const uint32_t zeros = ~value->value & value->knownBits;
        const uint32_t flips = value->flippedBits & ~value->knownBits;
        printf("  partial value known: ones=0x%08x zeros=0x%08x flips=0x%08x\n", ones, zeros, flips);

        // Check if the sequence of instructions contains an ORR (if ones is non-zero), BIC (if zeros is non-zero)
        // and/or EOR (if flips is non-zero), and that the first consumed variable is value->source and the last output
        // variable is var.
        match = BitwiseOpsMatchState{ones, zeros, flips, value->source, var.var, m_values}.Check(value);
        if (!match) {
            // Replace the last instruction with a BIC+ORR sequence, or one of the two
            IROp *currPos = m_emitter.GetCurrentOp();
            if (value->writerOp != nullptr) {
                // Writer op points to a non-const instruction
                auto __writerOpStr = value->writerOp->ToString();
                printf("    replaced '%s' with:\n", __writerOpStr.c_str());
                m_emitter.GoTo(value->writerOp);
                m_emitter.Overwrite();

                // Emit an ORR for all known one bits
                Variable result = value->source;
                if (ones != 0) {
                    result = m_emitter.BitwiseOr(result, ones, false);
                    auto __newOpStr = m_emitter.GetCurrentOp()->ToString();
                    printf("      '%s'\n", __newOpStr.c_str());
                }

                // Emit a BIC for all known zero bits
                if (zeros != 0) {
                    result = m_emitter.BitClear(result, zeros, false);
                    auto __newOpStr = m_emitter.GetCurrentOp()->ToString();
                    printf("      '%s'\n", __newOpStr.c_str());
                }

                // Emit a XOR for all unknown flipped bits
                if (flips != 0) {
                    result = m_emitter.BitwiseXor(result, flips, false);
                    auto __newOpStr = m_emitter.GetCurrentOp()->ToString();
                    printf("      '%s'\n", __newOpStr.c_str());
                }
                Assign(var, result);
                Substitute(var);

                m_emitter.GoTo(currPos);
            }
        }
    }

    // Erase previous instructions if changed
    if (!match) {
        value = GetValue(value->prev);
        while (value != nullptr) {
            if (value->writerOp != nullptr) {
                auto opStr = value->writerOp->ToString();
                printf("    erased '%s'\n", opStr.c_str());
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
        var = m_varSubsts[varIndex];
    }
}

void BasicPeepholeOptimizerPass::Substitute(VarOrImmArg &var) {
    if (!var.immediate) {
        Substitute(var.var);
    }
}

BasicPeepholeOptimizerPass::BitwiseOpsMatchState::BitwiseOpsMatchState(uint32_t ones, uint32_t zeros, uint32_t flips,
                                                                       Variable expectedInput, Variable expectedOutput,
                                                                       std::vector<Value> &values)
    : hasOnes(ones == 0)
    , hasZeros(zeros == 0)
    , hasFlips(flips == 0)
    , ones(ones)
    , zeros(zeros)
    , flips(flips)
    , expectedInput(expectedInput)
    , expectedOutput(expectedOutput)
    , values(values) {}

bool BasicPeepholeOptimizerPass::BitwiseOpsMatchState::Check(Value *value) {
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
