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

        const auto setCarry = op->setCarry;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setCarry) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(arm::Flags::C, static_cast<uint32_t>((*carry) ? arm::Flags::C : arm::Flags::None));
                if (*carry) {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::C);
                } else {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::None);
                }
            } else {
                ClearKnownHostFlags(arm::Flags::C);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setCarry = op->setCarry;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setCarry) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(arm::Flags::C, static_cast<uint32_t>((*carry) ? arm::Flags::C : arm::Flags::None));
                if (*carry) {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::C);
                } else {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::None);
                }
            } else {
                ClearKnownHostFlags(arm::Flags::C);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ASR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setCarry = op->setCarry;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setCarry) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(arm::Flags::C, static_cast<uint32_t>((*carry) ? arm::Flags::C : arm::Flags::None));
                if (*carry) {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::C);
                } else {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::None);
                }
            } else {
                ClearKnownHostFlags(arm::Flags::C);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);

        const bool setCarry = op->setCarry;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (setCarry) {
            if (carry.has_value()) {
                m_emitter.StoreFlags(arm::Flags::C, static_cast<uint32_t>((*carry) ? arm::Flags::C : arm::Flags::None));
                if (*carry) {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::C);
                } else {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::None);
                }
            } else {
                ClearKnownHostFlags(arm::Flags::C);
            }
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

            const bool setCarry = op->setCarry;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (setCarry) {
                m_emitter.StoreFlags(arm::Flags::C, static_cast<uint32_t>(carry ? arm::Flags::C : arm::Flags::None));
                if (carry) {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::C);
                } else {
                    SetKnownHostFlags(arm::Flags::C, arm::Flags::None);
                }
            }
        } else if (op->setCarry) {
            ClearKnownHostFlags(arm::Flags::C);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & op->rhs.imm.value;
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.Erase(op);
        }
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value | op->rhs.imm.value;
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value ^ op->rhs.imm.value;
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.Erase(op);
        }
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
    }
}

void ConstPropagationOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & ~op->rhs.imm.value;
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
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

        const arm::Flags flags = op->flags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.Erase(op);
        }
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
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

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
                const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
                SetKnownHostFlags(flags, setFlags);
            }
        } else {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        if (op->dst.var.IsPresent()) {
            m_emitter.Overwrite().Constant(op->dst, result);
        } else {
            m_emitter.Erase(op);
        }
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
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

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
                const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
                SetKnownHostFlags(flags, setFlags);
            }
        } else {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);
    Assign(op->dst, op->value);
    if (op->value.immediate) {
        const arm::Flags flags = op->flags;
        const uint32_t value = op->value.imm.value;
        m_emitter.Overwrite().Constant(op->dst, value);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, value);
            SetKnownHostFlags(flags, setFlags);
        }
    } else if (BitmaskEnum(op->flags).NoneOf(arm::kFlagsNZ)) {
        m_emitter.Overwrite().CopyVar(op->dst, op->value.var);
    } else {
        ClearKnownHostFlags(op->flags);
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = ~op->value.imm.value;
        Assign(op->dst, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
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
            m_emitter.StoreFlags(arm::Flags::Q, static_cast<uint32_t>(arm::Flags::Q));
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::Q);
        } else {
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::None);
        }
    } else {
        ClearKnownHostFlags(arm::Flags::Q);
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
            m_emitter.StoreFlags(arm::Flags::Q, static_cast<uint32_t>(arm::Flags::Q));
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::Q);
        } else {
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::None);
        }
    } else {
        ClearKnownHostFlags(arm::Flags::Q);
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result = (int32_t)op->lhs.imm.value * (int32_t)op->rhs.imm.value;
            Assign(op->dst, result);

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, (uint32_t)result);
                SetKnownHostFlags(flags, setFlags);
            }
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            Assign(op->dst, result);

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
                SetKnownHostFlags(flags, setFlags);
            }
        }
    } else {
        ClearKnownHostFlags(arm::kFlagsNZ);
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

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dstLo, result >> 0ll);
            m_emitter.Constant(op->dstHi, result >> 32ll);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, (uint64_t)result);
                SetKnownHostFlags(flags, setFlags);
            }
        } else {
            auto result = (uint64_t)op->lhs.imm.value * (uint64_t)op->rhs.imm.value;
            Assign(op->dstLo, result >> 0ull);
            Assign(op->dstHi, result >> 32ull);

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dstLo, result >> 0ull);
            m_emitter.Constant(op->dstHi, result >> 32ull);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
                SetKnownHostFlags(flags, setFlags);
            }
        }
    } else {
        ClearKnownHostFlags(arm::kFlagsNZ);
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

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dstLo, result >> 0ull);
        m_emitter.Constant(op->dstHi, result >> 32ull);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(arm::kFlagsNZ);
    }
}

void ConstPropagationOptimizerPass::Process(IRStoreFlagsOp *op) {
    Substitute(op->values);
    if (op->values.immediate) {
        m_knownHostFlagsMask |= op->flags;
        m_knownHostFlagsValues &= ~op->flags;
        m_knownHostFlagsValues |= static_cast<arm::Flags>(op->values.imm.value);
    } else {
        m_knownHostFlagsMask &= ~op->flags;
        m_knownHostFlagsValues &= ~op->flags;
    }
}

void ConstPropagationOptimizerPass::Process(IRLoadFlagsOp *op) {
    Substitute(op->srcCPSR);

    const arm::Flags mask = op->flags;
    if (BitmaskEnum(m_knownHostFlagsMask).AllOf(mask)) {
        auto hostFlags = m_knownHostFlagsValues & mask;
        if (op->srcCPSR.immediate) {
            auto cpsr = static_cast<arm::Flags>(op->srcCPSR.imm.value);
            cpsr &= ~mask;
            cpsr |= hostFlags;
            m_emitter.Overwrite().Constant(op->dstCPSR, static_cast<uint32_t>(cpsr));
        } else {
            const auto srcCPSR = op->srcCPSR;
            const auto dstCPSR = op->dstCPSR;

            m_emitter.Overwrite();
            auto cpsr = m_emitter.BitClear(srcCPSR, static_cast<uint32_t>(mask), false);
            cpsr = m_emitter.BitwiseOr(cpsr, static_cast<uint32_t>(hostFlags), false);
            m_emitter.CopyVar(dstCPSR, cpsr);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    if (op->srcCPSR.immediate) {
        if (BitmaskEnum(m_knownHostFlagsMask).AllOf(arm::Flags::Q)) {
            const auto srcCPSR = op->srcCPSR;
            const auto dstCPSR = op->dstCPSR;

            m_emitter.Overwrite();
            if (BitmaskEnum(m_knownHostFlagsValues).AnyOf(arm::Flags::Q)) {
                auto cpsr = m_emitter.BitwiseOr(srcCPSR, static_cast<uint32_t>(arm::Flags::Q), false);
                m_emitter.CopyVar(dstCPSR, cpsr);
            } else {
                auto cpsr = m_emitter.BitClear(srcCPSR, static_cast<uint32_t>(arm::Flags::Q), false);
                m_emitter.CopyVar(dstCPSR, cpsr);
            }
        }
    }
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
    if (BitmaskEnum(m_knownHostFlagsMask).AnyOf(arm::Flags::C)) {
        return BitmaskEnum(m_knownHostFlagsValues).AnyOf(arm::Flags::C);
    } else {
        return std::nullopt;
    }
}

void ConstPropagationOptimizerPass::SetKnownHostFlags(arm::Flags mask, arm::Flags values) {
    m_knownHostFlagsMask |= mask;
    m_knownHostFlagsValues &= ~mask;
    m_knownHostFlagsValues |= values & mask;
}

void ConstPropagationOptimizerPass::ClearKnownHostFlags(arm::Flags mask) {
    m_knownHostFlagsMask &= ~mask;
    m_knownHostFlagsValues &= ~mask;
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

void ConstPropagationOptimizerPass::Assign(VariableArg var, arm::Flags mask, arm::Flags flags) {
    if (!var.var.IsPresent()) {
        return;
    }
    if (mask == arm::Flags::None) {
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
