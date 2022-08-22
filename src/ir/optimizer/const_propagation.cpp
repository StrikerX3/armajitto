#include "const_propagation.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <unordered_map>
#include <utility>

#include <cstdio>
#include <format>

namespace armajitto::ir {

void ConstPropagationOptimizerPass::PreProcess() {
    // PC is known at entry
    Assign(arm::GPR::PC, m_emitter.BasePC());
}

void ConstPropagationOptimizerPass::Process(IRGetRegisterOp *op) {
    auto &srcSubst = GetGPRSubstitution(op->src);
    if (srcSubst.IsConstant()) {
        Assign(op->dst, srcSubst.constant);
        DefineCPSRBits(op->dst, ~0, srcSubst.constant);
        m_emitter.Overwrite().Constant(op->dst.var, srcSubst.constant);
    } else {
        UndefineCPSRBits(op->dst, ~0);
        if (srcSubst.IsVariable()) {
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
    InitCPSRBits(op->dst);
}

void ConstPropagationOptimizerPass::Process(IRSetCPSROp *op) {
    Substitute(op->src);
    auto opStr = op->ToString();
    printf("now checking:  %s\n", opStr.c_str());
    if (op->src.immediate) {
        // Immediate value makes every bit known
        m_knownCPSRBits.mask = ~0;
        m_knownCPSRBits.values = op->src.imm.value;
        printf("  immediate!\n");
        UpdateCPSRBitWrites(op, ~0);
    } else if (!op->src.immediate && op->src.var.var.IsPresent()) {
        // Check for derived values
        const auto index = op->src.var.var.Index();
        if (index < m_cpsrBitsPerVar.size()) {
            auto &bits = m_cpsrBitsPerVar[index];
            if (bits.valid) {
                // Check for differences between current CPSR value and the one coming from the variable
                const uint32_t maskDelta = (bits.knownBits.mask ^ m_knownCPSRBits.mask) | bits.undefinedBits;
                const uint32_t valsDelta = (bits.knownBits.values ^ m_knownCPSRBits.values) | bits.undefinedBits;
                printf("  found valid entry! delta: 0x%08x 0x%08x\n", maskDelta, valsDelta);
                if (m_knownCPSRBits.mask != 0 && maskDelta == 0 && valsDelta == 0) {
                    // All masked bits are equal; CPSR value has not changed
                    printf("    no changes! erasing instruction\n");
                    m_emitter.Erase(op);
                } else {
                    // Either the mask or the value (or both) changed
                    printf("    applying changes -> 0x%08x 0x%08x\n", bits.changedBits.mask, bits.changedBits.values);
                    m_knownCPSRBits = bits.knownBits;
                    UpdateCPSRBitWrites(op, bits.changedBits.mask | bits.undefinedBits);
                }
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRGetSPSROp *op) {
    UndefineCPSRBits(op->dst, ~0);
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
        DefineCPSRBits(op->dst, ~0, result);

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
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ASR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    Substitute(op->value);
    auto carryFlag = GetCarryFlag();
    if (op->value.immediate && carryFlag.has_value()) {
        auto [result, carry] = arm::RRX(op->value.imm.value, *carryFlag);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
    } else {
        if (op->setCarry) {
            ClearKnownHostFlags(arm::Flags::C);
        }
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & op->rhs.imm.value;
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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

        // AND clears all zero bits
        if (op->lhs.immediate) {
            DeriveCPSRBits(op->dst, op->rhs.var, ~op->lhs.imm.value, 0);
        } else if (op->rhs.immediate) {
            DeriveCPSRBits(op->dst, op->lhs.var, ~op->rhs.imm.value, 0);
        } else {
            UndefineCPSRBits(op->dst, ~0);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value | op->rhs.imm.value;
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);

        // OR sets all one bits
        if (op->lhs.immediate) {
            DeriveCPSRBits(op->dst, op->rhs.var, op->lhs.imm.value, op->lhs.imm.value);
        } else if (op->rhs.immediate) {
            DeriveCPSRBits(op->dst, op->lhs.var, op->rhs.imm.value, op->rhs.imm.value);
        } else {
            UndefineCPSRBits(op->dst, ~0);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value ^ op->rhs.imm.value;
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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

        // XOR flips all one bits
        if (op->lhs.immediate) {
            UndefineCPSRBits(op->dst, op->lhs.imm.value);
        } else if (op->rhs.immediate) {
            UndefineCPSRBits(op->dst, op->rhs.imm.value);
        } else {
            UndefineCPSRBits(op->dst, ~0);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & ~op->rhs.imm.value;
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);

        // BIC clears all one bits
        if (op->lhs.immediate) {
            DeriveCPSRBits(op->dst, op->rhs.var, op->lhs.imm.value, 0);
        } else if (op->rhs.immediate) {
            DeriveCPSRBits(op->dst, op->lhs.var, op->rhs.imm.value, 0);
        } else {
            UndefineCPSRBits(op->dst, ~0);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = std::countl_zero(op->value.imm.value);
        Assign(op->dst, result);
        m_emitter.Overwrite().Constant(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    auto carryFlag = GetCarryFlag();
    if (op->lhs.immediate && op->rhs.immediate && carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::ADC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

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
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    auto carryFlag = GetCarryFlag();
    if (op->lhs.immediate && op->rhs.immediate && carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::SBC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(flags, result, carry, overflow);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);
    Assign(op->dst, op->value);
    if (op->value.immediate) {
        const arm::Flags flags = op->flags;
        const uint32_t value = op->value.imm.value;
        DefineCPSRBits(op->dst, ~0, value);
        m_emitter.Overwrite().Constant(op->dst, value);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, value);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        CopyCPSRBits(op->dst, op->value.var);
        if (BitmaskEnum(op->flags).NoneOf(arm::kFlagsNZ)) {
            m_emitter.Overwrite().CopyVar(op->dst, op->value.var);
        } else {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = ~op->value.imm.value;
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dst, result);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(op->flags);
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, q] = arm::Saturate((int64_t)op->lhs.imm.value + op->rhs.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        m_emitter.Overwrite().Constant(op->dst, result);
        if (q) {
            m_emitter.StoreFlags(arm::Flags::Q, static_cast<uint32_t>(arm::Flags::Q));
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::Q);
        } else {
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::None);
        }
    } else {
        ClearKnownHostFlags(arm::Flags::Q);
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, q] = arm::Saturate((int64_t)op->lhs.imm.value - op->rhs.imm.value);
        Assign(op->dst, result);
        DefineCPSRBits(op->dst, ~0, result);

        m_emitter.Overwrite().Constant(op->dst, result);
        if (q) {
            m_emitter.StoreFlags(arm::Flags::Q, static_cast<uint32_t>(arm::Flags::Q));
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::Q);
        } else {
            SetKnownHostFlags(arm::Flags::Q, arm::Flags::None);
        }
    } else {
        ClearKnownHostFlags(arm::Flags::Q);
        UndefineCPSRBits(op->dst, ~0);
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result = (int32_t)op->lhs.imm.value * (int32_t)op->rhs.imm.value;
            Assign(op->dst, result);
            DefineCPSRBits(op->dst, ~0, result);

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, (uint32_t)result);
                SetKnownHostFlags(flags, setFlags);
            }
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            Assign(op->dst, result);
            DefineCPSRBits(op->dst, ~0, result);

            const arm::Flags flags = op->flags;
            m_emitter.Overwrite().Constant(op->dst, result);
            if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
                SetKnownHostFlags(flags, setFlags);
            }
        }
    } else {
        ClearKnownHostFlags(arm::kFlagsNZ);
        UndefineCPSRBits(op->dst, ~0);
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
            DefineCPSRBits(op->dstLo, ~0, result);
            DefineCPSRBits(op->dstHi, ~0, result);

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
            DefineCPSRBits(op->dstLo, ~0, result);
            DefineCPSRBits(op->dstHi, ~0, result);

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
        UndefineCPSRBits(op->dstLo, ~0);
        UndefineCPSRBits(op->dstHi, ~0);
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
        DefineCPSRBits(op->dstLo, ~0, result);
        DefineCPSRBits(op->dstHi, ~0, result);

        const arm::Flags flags = op->flags;
        m_emitter.Overwrite().Constant(op->dstLo, result >> 0ull);
        m_emitter.Constant(op->dstHi, result >> 32ull);
        if (BitmaskEnum(flags).AnyOf(arm::kFlagsNZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(flags, result);
            SetKnownHostFlags(flags, setFlags);
        }
    } else {
        ClearKnownHostFlags(arm::kFlagsNZ);
        UndefineCPSRBits(op->dstLo, ~0);
        UndefineCPSRBits(op->dstHi, ~0);
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
        DefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(op->flags), static_cast<uint32_t>(hostFlags));
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
    } else {
        UndefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(op->flags));
    }
}

void ConstPropagationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    if (op->srcCPSR.immediate && BitmaskEnum(m_knownHostFlagsMask).AllOf(arm::Flags::Q)) {
        DefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(arm::Flags::Q),
                       static_cast<uint32_t>(m_knownHostFlagsValues & arm::Flags::Q));
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
    } else {
        UndefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(arm::Flags::Q));
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

void ConstPropagationOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    UndefineCPSRBits(op->dstValue, ~0);
}

void ConstPropagationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
}

void ConstPropagationOptimizerPass::Process(IRConstantOp *op) {
    Assign(op->dst, op->value);
    DefineCPSRBits(op->dst, ~0, op->value);
}

void ConstPropagationOptimizerPass::Process(IRCopyVarOp *op) {
    Substitute(op->var);
    Assign(op->dst, op->var);
    CopyCPSRBits(op->dst, op->var);
}

void ConstPropagationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    UndefineCPSRBits(op->dst, ~0);
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

void ConstPropagationOptimizerPass::ResizeCPSRBitsPerVar(size_t size) {
    if (m_cpsrBitsPerVar.size() <= size) {
        m_cpsrBitsPerVar.resize(size + 1);
    }
}

void ConstPropagationOptimizerPass::InitCPSRBits(VariableArg dst) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto index = dst.var.Index();
    ResizeCPSRBitsPerVar(index);
    CPSRBits &bits = m_cpsrBitsPerVar[index];
    bits.valid = true;
    bits.knownBits = m_knownCPSRBits;

    auto dstStr = dst.ToString();
    printf("%s = [cpsr] bits=0x%08x vals=0x%08x\n", dstStr.c_str(), m_knownCPSRBits.mask, m_knownCPSRBits.values);
}

void ConstPropagationOptimizerPass::DeriveCPSRBits(VariableArg dst, VariableArg src, uint32_t mask, uint32_t value) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    if (srcIndex >= m_cpsrBitsPerVar.size()) {
        // This shouldn't happen
        return;
    }
    auto &srcBits = m_cpsrBitsPerVar[srcIndex];
    if (!srcBits.valid) {
        // Not a CPSR value; don't care
        return;
    }
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.knownBits.mask = srcBits.knownBits.mask | mask;
    dstBits.knownBits.values = (srcBits.knownBits.values & ~mask) | (value & mask);
    dstBits.changedBits.mask = srcBits.changedBits.mask | mask;
    dstBits.changedBits.values = (srcBits.changedBits.values & ~mask) | (value & mask);
    dstBits.undefinedBits = srcBits.undefinedBits;

    auto dstStr = dst.ToString();
    auto srcStr = src.ToString();
    printf("%s = [derived] src=%s bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n",
           dstStr.c_str(), srcStr.c_str(), dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask,
           dstBits.changedBits.values, dstBits.undefinedBits);
}

void ConstPropagationOptimizerPass::CopyCPSRBits(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    if (srcIndex >= m_cpsrBitsPerVar.size()) {
        // This shouldn't happen
        return;
    }
    auto &srcBits = m_cpsrBitsPerVar[srcIndex];
    if (!srcBits.valid) {
        // Not a CPSR value; don't care
        return;
    }
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits = srcBits;

    auto dstStr = dst.ToString();
    auto srcStr = src.ToString();
    printf("%s = [copied] src=%s bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n", dstStr.c_str(),
           srcStr.c_str(), dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask,
           dstBits.changedBits.values, dstBits.undefinedBits);
}

void ConstPropagationOptimizerPass::DefineCPSRBits(VariableArg dst, uint32_t mask, uint32_t value) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.knownBits.mask = mask;
    dstBits.knownBits.values = value & mask;
    dstBits.changedBits.mask = mask;
    dstBits.changedBits.values = value & mask;

    auto dstStr = dst.ToString();
    printf("%s = [defined] bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n", dstStr.c_str(),
           dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask, dstBits.changedBits.values,
           dstBits.undefinedBits);
}

void ConstPropagationOptimizerPass::UndefineCPSRBits(VariableArg dst, uint32_t mask) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.undefinedBits = mask;

    auto dstStr = dst.ToString();
    printf("%s = [undefined] bits=0x%08x\n", dstStr.c_str(), mask);
}

void ConstPropagationOptimizerPass::UpdateCPSRBitWrites(IROp *op, uint32_t mask) {
    for (uint32_t i = 0; i < 32; i++) {
        const uint32_t bit = (1 << i);
        if (!(mask & bit)) {
            continue;
        }

        // Check for previous write
        auto *prevOp = m_cpsrBitWrites[i];
        if (prevOp != nullptr) {
            // Clear bit from mask
            auto &writeMask = m_cpsrBitWriteMasks[prevOp];
            writeMask &= ~bit;
            if (writeMask == 0) {
                // Instruction no longer writes anything useful; erase it
                auto str = prevOp->ToString();
                printf("    erasing %s\n", str.c_str());
                m_emitter.Erase(prevOp);
                m_cpsrBitWriteMasks.erase(prevOp);
            }
        }

        // Update reference to this instruction and update mask
        m_cpsrBitWrites[i] = op;
        m_cpsrBitWriteMasks[op] |= bit;
    }
}

/*void ConstPropagationOptimizerPass::TrackCPSRBits(VariableArg dst, uint32_t mask, uint32_t values, IROp *op) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto index = dst.var.Index();
    ResizeCPSRBitsPerVar(index);
    m_cpsrBitsPerVar[index].valid = true;
    m_cpsrBitsPerVar[index].knownBits.mask = mask;
    m_cpsrBitsPerVar[index].knownBits.values = values;
    for (uint32_t i = 0; i < 32; i++) {
        if (mask & (1 << i)) {
            m_cpsrBitsPerVar[index].ops[i] = op;
        }
    }
    auto dstStr = dst.ToString();
    auto maskStr = std::format("0x{:x}", mask);
    auto valsStr = std::format("0x{:x}", values);
    printf("%s = [known imm] mask=%s vals=%s op=0x%p\n", dstStr.c_str(), maskStr.c_str(), valsStr.c_str(), op);
}

void ConstPropagationOptimizerPass::TrackCPSRBits(VariableArg dst, uint32_t mask, VariableArg src, IROp *op) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    // TODO: implement
}

void ConstPropagationOptimizerPass::TrackCPSRBits(VariableArg dst, VariableArg src, IROp *op) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    auto dstStr = dst.ToString();
    auto srcStr = src.ToString();
    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    if (srcIndex >= m_cpsrBitsPerVar.size()) {
        // This shouldn't happen
        return;
    }
    m_cpsrBitsPerVar[dstIndex].src = src.var;
    m_cpsrBitsPerVar[dstIndex].knownBits = m_cpsrBitsPerVar[srcIndex].knownBits;
    for (uint32_t i = 0; i < 32; i++) {
        if (m_cpsrBitsPerVar[dstIndex].knownBits.mask & (1 << i)) {
            m_cpsrBitsPerVar[dstIndex].ops[i] = op;
        }
    }
    printf("%s = [copy var] %s op=0x%p\n", dstStr.c_str(), srcStr.c_str(), op);
}

void ConstPropagationOptimizerPass::TrackCPSRBits(VariableArg dst, uint32_t mask, IROp *op) {
    if (!dst.var.IsPresent()) {
        return;
    }
    auto dstStr = dst.ToString();
    auto maskStr = std::format("0x{:x}", mask);
    printf("%s = [unknown] mask=%s op=0x%p\n", dstStr.c_str(), maskStr.c_str(), op);
}*/

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
