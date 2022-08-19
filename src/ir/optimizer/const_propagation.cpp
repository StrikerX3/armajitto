#include "const_propagation.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

inline bool Saturate(const int64_t value, int32_t &result) {
    constexpr int64_t min = (int64_t)std::numeric_limits<int32_t>::min();
    constexpr int64_t max = (int64_t)std::numeric_limits<int32_t>::max();
    result = std::clamp(value, min, max);
    return (result != value);
}

inline std::pair<uint32_t, std::optional<bool>> LSL(uint32_t value, uint32_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        return {0, (offset == 32) && (value & 1)};
    }
    bool carry = (value >> (32 - offset)) & 1;
    return {value << offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> LSR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        return {0, (offset == 32) && (value >> 31)};
    }
    bool carry = (value >> (offset - 1)) & 1;
    return {value >> offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> ASR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        bool carry = value >> 31;
        return {(int32_t)value >> 31, carry};
    }
    bool carry = (value >> (offset - 1)) & 1;
    return {(int32_t)value >> offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> ROR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }

    value = std::rotr(value, offset & 0x1F);
    bool carry = value >> 31;
    return {value, carry};
}

inline std::pair<uint32_t, bool> RRX(uint32_t value, bool carry) {
    uint32_t msb = carry << 31;
    carry = value & 1;
    return {(value >> 1) | msb, carry};
}

void ConstPropagationOptimizerPass::Optimize(Emitter &emitter) {
    for (emitter.SetCursorPos(0); !emitter.IsCursorAtEnd(); emitter.MoveCursor(1)) {
        auto *op = emitter.GetCurrentOp();
        if (op == nullptr) {
            assert(false);
            // shouldn't happen
            continue;
        }

        switch (op->GetType()) {
        case IROpcodeType::GetRegister: Process(*Cast<IRGetRegisterOp>(op)); break;
        case IROpcodeType::SetRegister: Process(*Cast<IRSetRegisterOp>(op)); break;
        case IROpcodeType::GetCPSR: Process(*Cast<IRGetCPSROp>(op)); break;
        case IROpcodeType::SetCPSR: Process(*Cast<IRSetCPSROp>(op)); break;
        case IROpcodeType::GetSPSR: Process(*Cast<IRGetSPSROp>(op)); break;
        case IROpcodeType::SetSPSR: Process(*Cast<IRSetSPSROp>(op)); break;
        case IROpcodeType::MemRead: Process(*Cast<IRMemReadOp>(op)); break;
        case IROpcodeType::MemWrite: Process(*Cast<IRMemWriteOp>(op)); break;
        case IROpcodeType::Preload: Process(*Cast<IRPreloadOp>(op)); break;
        case IROpcodeType::LogicalShiftLeft: Process(*Cast<IRLogicalShiftLeftOp>(op)); break;
        case IROpcodeType::LogicalShiftRight: Process(*Cast<IRLogicalShiftRightOp>(op)); break;
        case IROpcodeType::ArithmeticShiftRight: Process(*Cast<IRArithmeticShiftRightOp>(op)); break;
        case IROpcodeType::RotateRight: Process(*Cast<IRRotateRightOp>(op)); break;
        case IROpcodeType::RotateRightExtend: Process(*Cast<IRRotateRightExtendOp>(op)); break;
        case IROpcodeType::BitwiseAnd: Process(*Cast<IRBitwiseAndOp>(op)); break;
        case IROpcodeType::BitwiseOr: Process(*Cast<IRBitwiseOrOp>(op)); break;
        case IROpcodeType::BitwiseXor: Process(*Cast<IRBitwiseXorOp>(op)); break;
        case IROpcodeType::BitClear: Process(*Cast<IRBitClearOp>(op)); break;
        case IROpcodeType::CountLeadingZeros: Process(*Cast<IRCountLeadingZerosOp>(op)); break;
        case IROpcodeType::Add: Process(*Cast<IRAddOp>(op)); break;
        case IROpcodeType::AddCarry: Process(*Cast<IRAddCarryOp>(op)); break;
        case IROpcodeType::Subtract: Process(*Cast<IRSubtractOp>(op)); break;
        case IROpcodeType::SubtractCarry: Process(*Cast<IRSubtractCarryOp>(op)); break;
        case IROpcodeType::Move: Process(*Cast<IRMoveOp>(op)); break;
        case IROpcodeType::MoveNegated: Process(*Cast<IRMoveNegatedOp>(op)); break;
        case IROpcodeType::SaturatingAdd: Process(*Cast<IRSaturatingAddOp>(op)); break;
        case IROpcodeType::SaturatingSubtract: Process(*Cast<IRSaturatingSubtractOp>(op)); break;
        case IROpcodeType::Multiply: Process(*Cast<IRMultiplyOp>(op)); break;
        case IROpcodeType::MultiplyLong: Process(*Cast<IRMultiplyLongOp>(op)); break;
        case IROpcodeType::AddLong: Process(*Cast<IRAddLongOp>(op)); break;
        case IROpcodeType::StoreFlags: Process(*Cast<IRStoreFlagsOp>(op)); break;
        case IROpcodeType::UpdateFlags: Process(*Cast<IRUpdateFlagsOp>(op)); break;
        case IROpcodeType::UpdateStickyOverflow: Process(*Cast<IRUpdateStickyOverflowOp>(op)); break;
        case IROpcodeType::Branch: Process(*Cast<IRBranchOp>(op)); break;
        case IROpcodeType::BranchExchange: Process(*Cast<IRBranchExchangeOp>(op)); break;
        case IROpcodeType::LoadCopRegister: Process(*Cast<IRLoadCopRegisterOp>(op)); break;
        case IROpcodeType::StoreCopRegister: Process(*Cast<IRStoreCopRegisterOp>(op)); break;
        case IROpcodeType::Constant: Process(*Cast<IRConstantOp>(op)); break;
        case IROpcodeType::CopyVar: Process(*Cast<IRCopyVarOp>(op)); break;
        case IROpcodeType::GetBaseVectorAddress: Process(*Cast<IRGetBaseVectorAddressOp>(op)); break;
        default: break;
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRGetRegisterOp *op) {
    auto &srcSubst = GetGPRSubstitution(op->src);
    if (srcSubst.IsKnown()) {
        if (srcSubst.IsConstant()) {
            Assign(op->dst, srcSubst.constant);
            // TODO: replace instruction with Constant
        } else if (srcSubst.IsVariable()) {
            Assign(op->dst, srcSubst.variable);
            // TODO: replace instruction with CopyVar
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSetRegisterOp *op) {
    Substitute(op->src);
    Assign(op->dst, op->src);
}

void ConstPropagationOptimizerPass::Process(IRGetCPSROp *op) {
    // Nothing to do
}

void ConstPropagationOptimizerPass::Process(IRSetCPSROp *op) {
    Substitute(op->src);
}

void ConstPropagationOptimizerPass::Process(IRGetSPSROp *op) {
    // Nothing to do
}

void ConstPropagationOptimizerPass::Process(IRSetSPSROp *op) {
    Substitute(op->src);
}

void ConstPropagationOptimizerPass::Process(IRMemReadOp *op) {
    Substitute(op->address);
}

void ConstPropagationOptimizerPass::Process(IRMemWriteOp *op) {
    Substitute(op->src);
    Substitute(op->address);
}

void ConstPropagationOptimizerPass::Process(IRPreloadOp *op) {
    Substitute(op->address);
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = LSL(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        if (op->setFlags && carry.has_value()) {
            // TODO: update carry flag
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = LSR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        if (op->setFlags && carry.has_value()) {
            // TODO: update carry flag
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = ASR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        if (op->setFlags && carry.has_value()) {
            // TODO: update carry flag
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = ROR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        if (op->setFlags && carry.has_value()) {
            // TODO: update carry flag
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    if (op->value.immediate /* TODO: && carryKnown */) {
        // TODO: get carry input and:
        // auto [result, carry] = RRX(op->value.imm.value, carryIn);
        // Assign(op->dst, result);
        // if (op->setFlags) {
        //     // TODO: update carry flag
        // }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value | op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value ^ op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & ~op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = std::countl_zero(op->value.imm.value);
        Assign(op->dst, result);
        // TODO: replace instruction somehow
    }
}

void ConstPropagationOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value + op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate /* TODO: && carryKnown */) {
        // TODO: calculate result (including flags if op->setFlags) and assign to dst:
        // Assign(op->dst, result);
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value - op->rhs.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate /* TODO: && carryKnown */) {
        // TODO: calculate result (including flags if op->setFlags) and assign to dst:
        // Assign(op->dst, result);
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);
    Assign(op->dst, op->value);
    if (op->value.immediate) {
        if (op->setFlags) {
            // TODO: calculate flags
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = ~op->value.imm.value;
        Assign(op->dst, result);
        if (op->setFlags) {
            // TODO: calculate flags
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        int32_t result{};
        bool q = Saturate((int64_t)op->lhs.imm.value + op->rhs.imm.value, result);
        Assign(op->dst, result);

        // TODO: replace instruction somehow
        if (q) {
            // TODO: emitter.StoreFlags(Flags::Q, static_cast<uint32_t>(Flags::Q));
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        int32_t result{};
        bool q = Saturate((int64_t)op->lhs.imm.value - op->rhs.imm.value, result);
        Assign(op->dst, result);

        // TODO: replace instruction somehow
        if (q) {
            // TODO: emitter.StoreFlags(Flags::Q, static_cast<uint32_t>(Flags::Q));
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result = (int32_t)op->lhs.imm.value * (int32_t)op->rhs.imm.value;
            Assign(op->dst, result);
            if (op->setFlags) {
                // TODO: calculate flags based on result
            }
            // TODO: replace instruction somehow; watch out for flags
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            Assign(op->dst, result);
            if (op->setFlags) {
                // TODO: calculate flags based on result
            }
            // TODO: replace instruction somehow; watch out for flags
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        // TODO: calculate result (including flags if op->setFlags) and assign to dstLo and dstHi:
        // Assign(op->dstLo, result >> 0ull);  // or 0ll if signedMul
        // Assign(op->dstHu, result >> 32ull); // or 32ll if signedMul
        // TODO: replace instruction somehow; watch out for flags

        if (op->signedMul) {
            auto result = (int64_t)op->lhs.imm.value * (int64_t)op->rhs.imm.value;
            Assign(op->dstLo, result >> 0ll);
            Assign(op->dstHi, result >> 32ll);
            if (op->setFlags) {
                // TODO: calculate flags based on result
            }
            // TODO: replace instruction somehow; watch out for flags
        } else {
            auto result = (uint64_t)op->lhs.imm.value * (uint64_t)op->rhs.imm.value;
            Assign(op->dstLo, result >> 0ull);
            Assign(op->dstHi, result >> 32ull);
            if (op->setFlags) {
                // TODO: calculate flags based on result
            }
            // TODO: replace instruction somehow; watch out for flags
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRAddLongOp *op) {
    Substitute(op->lhsLo);
    Substitute(op->lhsHi);
    Substitute(op->rhsLo);
    Substitute(op->rhsHi);
    if (op->lhsLo.immediate && op->lhsHi.immediate && op->rhsLo.immediate && op->rhsHi.immediate) {
        auto make64 = [](uint32_t lo, uint32_t hi) { return (uint64_t)lo | ((uint64_t)hi << 32ull); };
        uint64_t lhs = make64(op->lhsLo.imm.value, op->lhsHi.imm.value);
        uint64_t rhs = make64(op->rhsLo.imm.value, op->rhsHi.imm.value);
        uint64_t result = lhs + rhs;
        Assign(op->dstLo, result >> 0ull);
        Assign(op->dstHi, result >> 32ull);
        if (op->setFlags) {
            // TODO: calculate flags based on result
        }
        // TODO: replace instruction somehow; watch out for flags
    }
}

void ConstPropagationOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->srcCPSR);
    Substitute(op->values);
}

void ConstPropagationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    Substitute(op->srcCPSR);
}

void ConstPropagationOptimizerPass::Process(IRUpdateStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
}

void ConstPropagationOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
}

void ConstPropagationOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
}

void ConstPropagationOptimizerPass::Process(IRLoadCopRegisterOp *op) {}

void ConstPropagationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
}

void ConstPropagationOptimizerPass::Process(IRConstantOp *op) {
    Assign(op->dst, op->value);
}

void ConstPropagationOptimizerPass::Process(IRCopyVarOp *op) {
    Substitute(op->var);
    Assign(op->dst, op->var);
}

void ConstPropagationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {}

void ConstPropagationOptimizerPass::ResizeVarSubsts(size_t size) {
    if (m_varSubsts.size() <= size) {
        m_varSubsts.resize(size + 1);
    }
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, VariableArg value) {
    Assign(var, value.var);
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, ImmediateArg value) {
    Assign(var, value.value);
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, VarOrImmArg value) {
    if (value.immediate) {
        Assign(var, value.imm);
    } else {
        Assign(var, value.var);
    }
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, Variable value) {
    if (!var.var.IsPresent()) {
        return;
    }
    if (!value.IsPresent()) {
        return;
    }
    auto varIndex = var.var.Index();
    ResizeVarSubsts(varIndex);
    m_varSubsts[varIndex] = value;
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, uint32_t value) {
    if (!var.var.IsPresent()) {
        return;
    }
    auto varIndex = var.var.Index();
    ResizeVarSubsts(varIndex);
    m_varSubsts[varIndex] = value;
}

void ConstPropagationOptimizerPass::Assign(GPRArg gpr, VarOrImmArg value) {
    if (!value.immediate && !value.var.var.IsPresent()) {
        return;
    }
    auto &subst = GetGPRSubstitution(gpr);
    if (value.immediate) {
        subst = value.imm.value;
    } else {
        subst = value.var.var;
    }
}

void ConstPropagationOptimizerPass::Substitute(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }
    auto varIndex = var.var.Index();
    if (varIndex < m_varSubsts.size()) {
        m_varSubsts[varIndex].Substitute(var);
    }
}

void ConstPropagationOptimizerPass::Substitute(VarOrImmArg &var) {
    if (var.immediate) {
        return;
    }
    if (!var.var.var.IsPresent()) {
        return;
    }
    auto varIndex = var.var.var.Index();
    if (varIndex < m_varSubsts.size()) {
        m_varSubsts[varIndex].Substitute(var);
    }
}

auto ConstPropagationOptimizerPass::GetGPRSubstitution(GPRArg gpr) -> Value & {
    auto &arr = (gpr.userMode) ? m_userGPRSubsts : m_gprSubsts;
    auto index = static_cast<size_t>(gpr.gpr);
    return arr[index];
}

void ConstPropagationOptimizerPass::Value::Substitute(VariableArg &var) {
    switch (type) {
    case Type::Unknown: break;
    case Type::Constant: break; // Can't replace a VariableArg with a constant
    case Type::Variable: var.var = variable; break;
    }
}

void ConstPropagationOptimizerPass::Value::Substitute(VarOrImmArg &var) {
    switch (type) {
    case Type::Unknown: break;
    case Type::Constant: var = constant; break;
    case Type::Variable: var = variable; break;
    }
}

} // namespace armajitto::ir
