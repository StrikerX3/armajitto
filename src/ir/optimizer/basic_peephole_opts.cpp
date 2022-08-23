#include "basic_peephole_opts.hpp"

#include <cstdio>
#include <format>

namespace armajitto::ir {

void BasicPeepholeOptimizerPass::Process(IRSetRegisterOp *op) {
    Substitute(op->src);
    if (!op->src.immediate) {
        ConsumeValue(op->src.var);
    }
}

void BasicPeepholeOptimizerPass::Process(IRSetCPSROp *op) {
    Substitute(op->src);
    if (!op->src.immediate) {
        ConsumeValue(op->src.var);
    }
}

void BasicPeepholeOptimizerPass::Process(IRSetSPSROp *op) {
    Substitute(op->src);
}

void BasicPeepholeOptimizerPass::Process(IRMemReadOp *op) {
    Substitute(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRMemWriteOp *op) {
    Substitute(op->src);
    Substitute(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRPreloadOp *op) {
    Substitute(op->address);
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
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
        return;
    }

    // XOR flips all one bits
    if (auto pair = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *pair;
        if (auto optValue = GetValue(var)) {
            auto &value = *optValue;
            // All flipped bits must be known
            if ((value.knownBits | ~imm) != 0) {
                DeriveKnownBits(op->dst, var, imm, value.value ^ imm, op);
            }
        }
    }
}

void BasicPeepholeOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
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
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);

    // Cannot optimize if flags are affected
    if (op->flags != arm::Flags::None) {
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
        return;
    }

    // MVN inverts all bits
    if (!op->value.immediate) {
        if (auto optValue = GetValue(op->value.var)) {
            auto &value = *optValue;
            // All must be known
            if (value.knownBits == ~0) {
                DeriveKnownBits(op->dst, op->value.var, ~0, ~value.value, op);
            }
        }
    }
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRAddLongOp *op) {
    Substitute(op->lhsLo);
    Substitute(op->lhsHi);
    Substitute(op->rhsLo);
    Substitute(op->rhsHi);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->values);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLoadFlagsOp *op) {
    Substitute(op->srcCPSR);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
    // TODO: implement
}

void BasicPeepholeOptimizerPass::Process(IRConstantOp *op) {
    AssignConstant(op->dst, op->value);
}

void BasicPeepholeOptimizerPass::Process(IRCopyVarOp *op) {
    Substitute(op->var);
    CopyVariable(op->dst, op->var, op);
}

void BasicPeepholeOptimizerPass::ResizeValues(size_t index) {
    if (m_values.size() <= index) {
        m_values.resize(index + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

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

    auto __varStr = var.ToString();
    printf("assigned known bits: %s value=0x%08x -> known mask=0x%08x value=0x%08x\n", __varStr.c_str(), value,
           dstValue.knownBits, dstValue.value);
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
    auto &dstValue = m_values[dstIndex];
    dstValue = m_values[srcIndex];
    dstValue.prev = src.var;
    dstValue.writerOp = op;

    auto __varStr = var.ToString();
    auto __srcStr = src.ToString();
    printf("copied known bits: %s = %s -> known mask=0x%08x value=0x%08x\n", __varStr.c_str(), __srcStr.c_str(),
           dstValue.knownBits, dstValue.value);
}

void BasicPeepholeOptimizerPass::DeriveKnownBits(VariableArg var, VariableArg src, uint32_t mask, uint32_t value,
                                                 IROp *op) {
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
    } else {
        dstValue.source = src.var;
        dstValue.knownBits = mask;
        dstValue.value = value & mask;
    }

    auto __varStr = var.ToString();
    auto __srcStr = src.ToString();
    printf("derived known bits: %s = %s -- mask=0x%08x value=0x%08x -> known mask=0x%08x value=0x%08x\n",
           __varStr.c_str(), __srcStr.c_str(), mask, value, dstValue.knownBits, dstValue.value);
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
    auto __baseStr = value->prev.ToString();
    printf("consuming %s -> mask=0x%08x value=0x%08x src=%s prev=%s\n", __varStr.c_str(), value->knownBits,
           value->value, __srcStr.c_str(), __baseStr.c_str());

    // No need to replace a single instruction
    if (value->source == value->prev) {
        printf("  not going to replace one instruction\n");
        return;
    }

    if (value->knownBits == ~0) {
        // The entire value is known
        printf("  full value known!\n");

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
    } else if (value->knownBits != 0) {
        // Some of the bits are known
        const uint32_t ones = value->value & value->knownBits;
        const uint32_t zeros = ~value->value & value->knownBits;
        printf("  partial value known: ones=0x%08x zeros=0x%08x\n", ones, zeros);

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
            Assign(var, result);
            Substitute(var);

            m_emitter.GoTo(currPos);
        }
    }

    // Erase previous instructions
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
    if (varIndex < m_varSubsts.size()) {
        var = m_varSubsts[varIndex];
    }
}

void BasicPeepholeOptimizerPass::Substitute(VarOrImmArg &var) {
    if (!var.immediate) {
        Substitute(var.var);
    }
}

} // namespace armajitto::ir
