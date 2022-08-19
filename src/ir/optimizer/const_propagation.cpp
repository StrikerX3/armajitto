#include "const_propagation.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

bool ConstPropagationOptimizerPass::Optimize() {
    m_emitter.ClearDirtyFlag();

    m_emitter.SetCursorPos(0);
    while (!m_emitter.IsCursorAtEnd()) {
        auto *op = m_emitter.GetCurrentOp();
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

        if (!m_emitter.IsModifiedSinceLastCursorMove()) {
            m_emitter.MoveCursor(1);
        } else {
            m_emitter.ClearModifiedSinceLastCursorMove();
        }
    }

    return m_emitter.IsDirty();
}

void ConstPropagationOptimizerPass::Process(IRGetRegisterOp *op) {
    auto &srcSubst = GetGPRSubstitution(op->src);
    if (srcSubst.IsKnown()) {
        if (srcSubst.IsConstant()) {
            Assign(op->dst, srcSubst.constant);
            m_emitter.Overwrite().Constant(op->dst.var, srcSubst.constant);
        } else if (srcSubst.IsVariable()) {
            Assign(op->dst, srcSubst.variable);
            m_emitter.Overwrite().CopyVar(op->dst.var, srcSubst.variable);
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
    if (op->src.immediate) {
        m_carryFlag = (static_cast<Flags>(op->src.imm.value) & Flags::C) == Flags::C;
    } else {
        m_carryFlag = std::nullopt;
    }
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
        auto [result, carry] = arm::LSL(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
            }
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
            }
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ASR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
            }
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
            }
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    if (op->value.immediate && m_carryFlag.has_value()) {
        auto [result, carry] = arm::RRX(op->value.imm.value, *m_carryFlag);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>(carry ? Flags::C : Flags::None));
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & op->rhs.imm.value;
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.EraseNext();
        }
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value | op->rhs.imm.value;
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value ^ op->rhs.imm.value;
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.EraseNext();
        }
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & ~op->rhs.imm.value;
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = std::countl_zero(op->value.imm.value);
        Assign(op->dst, result);
        m_emitter.Overwrite().Constant(op->dst, result);
    }
}

void ConstPropagationOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.EraseNext();
        }
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate && m_carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::ADC(op->lhs.imm.value, op->rhs.imm.value, *m_carryFlag);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.EraseNext();
        }
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate && m_carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::SBC(op->lhs.imm.value, op->rhs.imm.value, *m_carryFlag);
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
            m_carryFlag = carry;
        }
    } else if (op->setFlags) {
        m_carryFlag = std::nullopt;
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);
    Assign(op->dst, op->value);
    if (op->value.immediate) {
        const bool setFlags = op->setFlags;
        const uint32_t value = op->value.imm.value;
        m_emitter.Overwrite().Constant(op->dst, value);
        if (setFlags) {
            m_emitter.SetNZ(value);
        }
    } else if (!op->setFlags) {
        m_emitter.Overwrite().CopyVar(op->dst, op->value.var);
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = ~op->value.imm.value;
        Assign(op->dst, result);

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, q] = arm::Saturate((int64_t)op->lhs.imm.value + op->rhs.imm.value);
        Assign(op->dst, result);

        m_emitter.Overwrite().Constant(op->dst, result);
        if (q) {
            m_emitter.StoreFlags(Flags::Q, static_cast<uint32_t>(Flags::Q));
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, q] = arm::Saturate((int64_t)op->lhs.imm.value - op->rhs.imm.value);
        Assign(op->dst, result);

        m_emitter.Overwrite().Constant(op->dst, result);
        if (q) {
            m_emitter.StoreFlags(Flags::Q, static_cast<uint32_t>(Flags::Q));
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

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setFlags) {
                m_emitter.SetNZ((uint32_t)result);
            }
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            Assign(op->dst, result);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setFlags) {
                m_emitter.SetNZ(result);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result = (int64_t)op->lhs.imm.value * (int64_t)op->rhs.imm.value;
            Assign(op->dstLo, result >> 0ll);
            Assign(op->dstHi, result >> 32ll);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dstLo, result >> 0ll);
            m_emitter.Constant(op->dstHi, result >> 32ll);
            if (setFlags) {
                m_emitter.SetNZ((uint64_t)result);
            }
        } else {
            auto result = (uint64_t)op->lhs.imm.value * (uint64_t)op->rhs.imm.value;
            Assign(op->dstLo, result >> 0ull);
            Assign(op->dstHi, result >> 32ull);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dstLo, result >> 0ull);
            m_emitter.Constant(op->dstHi, result >> 32ull);
            if (setFlags) {
                m_emitter.SetNZ(result);
            }
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

        const bool setFlags = op->setFlags;
        m_emitter.Overwrite().Constant(op->dstLo, result >> 0ull);
        m_emitter.Constant(op->dstHi, result >> 32ull);
        if (setFlags) {
            m_emitter.SetNZ(result);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->srcCPSR);
    Substitute(op->values);
    if (BitmaskEnum(op->flags).AnyOf(Flags::C)) {
        if (op->values.immediate) {
            m_carryFlag = (static_cast<Flags>(op->values.imm.value) & Flags::C) == Flags::C;
        } else {
            m_carryFlag = std::nullopt;
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    Substitute(op->srcCPSR);
    if (BitmaskEnum(op->flags).AnyOf(Flags::C)) {
        m_carryFlag = std::nullopt;
    }
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

void ConstPropagationOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    // Nothing to do
}

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

void ConstPropagationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    // Nothing to do
}

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
