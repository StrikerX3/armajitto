#include "const_propagation.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

void ConstPropagationOptimizerPass::PreProcess() {
    // PC is known at entry
    Assign(arm::GPR::PC, m_emitter.BasePC());
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

void ConstPropagationOptimizerPass::Process(IRSetCPSROp *op) {
    Substitute(op->src);
    if (op->src.immediate) {
        m_knownFlagsMask = Flags::N | Flags::Z | Flags::C | Flags::V | Flags::Q;
        m_knownFlagsValues = static_cast<Flags>(op->src.imm.value);
    } else if (auto subst = GetFlagsSubstitution(op->src.var)) {
        m_knownFlagsMask = subst->knownMask;
        m_knownFlagsValues = subst->flags;
    } else {
        m_knownFlagsMask = Flags::None;
    }
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
        if (setFlags && carry.has_value()) {
            m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
        }
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
        if (setFlags && carry.has_value()) {
            m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
        }
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
        if (setFlags && carry.has_value()) {
            m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
        }
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
        if (setFlags && carry.has_value()) {
            m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>((*carry) ? Flags::C : Flags::None));
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto carryFlag = GetCarryFlag();
        if (carryFlag.has_value()) {
            auto [result, carry] = arm::RRX(op->value.imm.value, *carryFlag);
            Assign(op->dst, result);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setFlags) {
                m_emitter.StoreFlags(Flags::C, static_cast<uint32_t>(carry ? Flags::C : Flags::None));
            }
        }
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
            m_emitter.Erase(op);
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
            m_emitter.Erase(op);
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
            m_emitter.Erase(op);
        }
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto carryFlag = GetCarryFlag();
        if (carryFlag.has_value()) {
            auto [result, carry, overflow] = arm::ADC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
            Assign(op->dst, result);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setFlags) {
                m_emitter.SetNZCV(result, carry, overflow);
            }
        }
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
            m_emitter.Erase(op);
        }
        if (setFlags) {
            m_emitter.SetNZCV(result, carry, overflow);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto carryFlag = GetCarryFlag();
        if (carryFlag.has_value()) {
            auto [result, carry, overflow] = arm::SBC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
            Assign(op->dst, result);

            const bool setFlags = op->setFlags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setFlags) {
                m_emitter.SetNZCV(result, carry, overflow);
            }
        }
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
    if (BitmaskEnum(op->flags).Any() && op->values.immediate) {
        Assign(op->dstCPSR, op->flags, static_cast<Flags>(op->values.imm.value));
    }
}

void ConstPropagationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    Substitute(op->srcCPSR);
}

void ConstPropagationOptimizerPass::Process(IRUpdateStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
}

void ConstPropagationOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
    Forget(arm::GPR::PC);
}

void ConstPropagationOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
    Forget(arm::GPR::PC);
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

std::optional<bool> ConstPropagationOptimizerPass::GetCarryFlag() {
    if (BitmaskEnum(m_knownFlagsMask).AnyOf(Flags::C)) {
        return BitmaskEnum(m_knownFlagsValues).AnyOf(Flags::C);
    } else {
        return std::nullopt;
    }
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

void ConstPropagationOptimizerPass::Substitute(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }
    auto varIndex = var.var.Index();
    if (varIndex < m_varSubsts.size()) {
        MarkDirty(m_varSubsts[varIndex].Substitute(var));
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
        MarkDirty(m_varSubsts[varIndex].Substitute(var));
    }
}

void ConstPropagationOptimizerPass::Assign(const GPRArg &gpr, VarOrImmArg value) {
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

void ConstPropagationOptimizerPass::Forget(const GPRArg &gpr) {
    GetGPRSubstitution(gpr) = {};
}

auto ConstPropagationOptimizerPass::GetGPRSubstitution(const GPRArg &gpr) -> Value & {
    auto index = MakeGPRIndex(gpr);
    return m_gprSubsts[index];
}

void ConstPropagationOptimizerPass::ResizeFlagsSubsts(size_t size) {
    if (m_flagsSubsts.size() <= size) {
        m_flagsSubsts.resize(size + 1);
    }
}

void ConstPropagationOptimizerPass::Assign(VariableArg var, Flags mask, Flags flags) {
    if (!var.var.IsPresent()) {
        return;
    }
    if (mask == Flags::None) {
        return;
    }
    auto varIndex = var.var.Index();
    ResizeFlagsSubsts(varIndex);
    m_flagsSubsts[varIndex] = {mask, flags};
}

auto ConstPropagationOptimizerPass::GetFlagsSubstitution(VariableArg var) -> FlagsValue * {
    if (!var.var.IsPresent()) {
        return nullptr;
    }
    auto varIndex = var.var.Index();
    if (varIndex < m_flagsSubsts.size()) {
        return &m_flagsSubsts[varIndex];
    } else {
        return nullptr;
    }
}

bool ConstPropagationOptimizerPass::Value::Substitute(VariableArg &var) {
    if (type == Type::Variable) {
        var.var = variable;
        return true;
    } else {
        return false;
    }
}

bool ConstPropagationOptimizerPass::Value::Substitute(VarOrImmArg &var) {
    switch (type) {
    case Type::Constant: var = constant; return true;
    case Type::Variable: var = variable; return true;
    default: return false;
    }
}

} // namespace armajitto::ir
