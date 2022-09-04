#include "const_propagation.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <unordered_map>
#include <utility>

namespace armajitto::ir {

ConstPropagationOptimizerPass::ConstPropagationOptimizerPass(Emitter &emitter, std::pmr::memory_resource &alloc)
    : OptimizerPassBase(emitter)
    , m_varSubsts(&alloc) {

    const uint32_t varCount = emitter.VariableCount();
    m_varSubsts.resize(varCount);
}

void ConstPropagationOptimizerPass::Reset() {
    std::fill(m_varSubsts.begin(), m_varSubsts.end(), Value{});
    m_gprSubsts.fill({});
    m_knownHostFlagsMask = arm::Flags::None;
    m_knownHostFlagsValues = arm::Flags::None;
}

void ConstPropagationOptimizerPass::PreProcess() {
    // PC is known at entry
    Assign(arm::GPR::PC, m_emitter.BasePC());
}

void ConstPropagationOptimizerPass::Process(IRGetRegisterOp *op) {
    auto &srcSubst = GetGPRSubstitution(op->src);
    if (srcSubst.IsConstant()) {
        Assign(op->dst, srcSubst.constant);
        m_emitter.Overwrite().Constant(op->dst.var, srcSubst.constant);
    } else if (srcSubst.IsVariable()) {
        Assign(op->dst, srcSubst.variable);
        m_emitter.Overwrite().CopyVar(op->dst.var, srcSubst.variable);
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
        m_emitter.Erase(op);
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
    } else if (op->amount.immediate) {
        // Replace LSL by zero with a copy of the value.
        // This does not affect the carry flag.
        if (op->amount.imm.value == 0) {
            Assign(op->dst, op->value.var);
            m_emitter.Erase(op);
        } else {
            ClearKnownHostFlags(arm::Flags::C);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        const bool setCarry = op->setCarry;
        Assign(op->dst, result);
        m_emitter.Erase(op);

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
    } else if (op->amount.immediate) {
        // Replace LSR by zero with a copy of the value.
        // This does not affect the carry flag.
        if (op->amount.imm.value == 0) {
            Assign(op->dst, op->value.var);
            m_emitter.Erase(op);
        } else {
            ClearKnownHostFlags(arm::Flags::C);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ASR(op->value.imm.value, op->amount.imm.value);
        const bool setCarry = op->setCarry;
        Assign(op->dst, result);
        m_emitter.Erase(op);

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
    } else if (op->amount.immediate) {
        // Replace ASR by zero with a copy of the value.
        // This does not affect the carry flag.
        if (op->amount.imm.value == 0) {
            Assign(op->dst, op->value.var);
            m_emitter.Erase(op);
        } else {
            ClearKnownHostFlags(arm::Flags::C);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightOp *op) {
    Substitute(op->value);
    Substitute(op->amount);
    if (op->value.immediate && op->amount.immediate) {
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        const bool setCarry = op->setCarry;
        Assign(op->dst, result);
        m_emitter.Erase(op);

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
    } else if (op->amount.immediate) {
        // Replace ROR by zero with a copy of the value.
        // This does not affect the carry flag.
        if (op->amount.imm.value == 0) {
            Assign(op->dst, op->value.var);
            m_emitter.Erase(op);
        } else {
            ClearKnownHostFlags(arm::Flags::C);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    Substitute(op->value);
    auto carryFlag = GetCarryFlag();
    if (op->value.immediate && carryFlag.has_value()) {
        auto [result, carry] = arm::RRX(op->value.imm.value, *carryFlag);
        const bool setCarry = op->setCarry;
        Assign(op->dst, result);
        m_emitter.Erase(op);

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

void ConstPropagationOptimizerPass::Process(IRBitwiseAndOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::NZ);

    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & op->rhs.imm.value;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (setFlags) {
            const arm::Flags flagsSet = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, flagsSet);
        }
    } else {
        if (op->lhs.immediate != op->rhs.immediate) {
            if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
                auto [immValue, var] = *optPair;

                // Replace AND by 0xFFFFFFFF with a copy of the variable
                // Replace AND by 0x00000000 with the constant 0
                if (immValue == ~0) {
                    Assign(op->dst, var);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, var, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                } else if (immValue == 0) {
                    Assign(op->dst, 0);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, 0, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                }
            }
        } else if (op->lhs.var == op->rhs.var) {
            // Replace AND between the same variables with a copy of the variable
            Assign(op->dst, op->lhs.var);
            if (setFlags) {
                m_emitter.Overwrite().Move(op->dst, op->lhs.var, true);
            } else {
                m_emitter.Erase(op);
            }
        }

        if (setFlags) {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseOrOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::NZ);

    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value | op->rhs.imm.value;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (setFlags) {
            const arm::Flags flagsSet = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, flagsSet);
        }
    } else {
        if (op->lhs.immediate != op->rhs.immediate) {
            if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
                auto [immValue, var] = *optPair;

                // Replace OR by 0xFFFFFFFF with the constant 0xFFFFFFFF
                // Replace OR by 0x00000000 with a copy of the variable
                if (immValue == 0) {
                    Assign(op->dst, var);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, var, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                } else if (immValue == ~0) {
                    Assign(op->dst, ~0);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, ~0, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                }
            }
        } else if (op->lhs.var == op->rhs.var) {
            // Replace OR between the same variables with a copy of the variable
            Assign(op->dst, op->lhs.var);
            if (setFlags) {
                m_emitter.Overwrite().Move(op->dst, op->lhs.var, true);
            } else {
                m_emitter.Erase(op);
            }
        }

        if (setFlags) {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitwiseXorOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::NZ);

    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value ^ op->rhs.imm.value;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (setFlags) {
            const arm::Flags flagsSet = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, flagsSet);
        }
    } else {
        if (op->lhs.immediate != op->rhs.immediate) {
            if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
                auto [immValue, var] = *optPair;

                // Replace XOR by 0x00000000 with a copy of the variable
                if (immValue == 0) {
                    Assign(op->dst, var);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, var, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                }
            }
        } else if (op->lhs.var == op->rhs.var) {
            // Replace XOR between the same variables with zero
            Assign(op->dst, 0);
            if (setFlags) {
                m_emitter.Overwrite().Move(op->dst, 0, true);
            } else {
                m_emitter.Erase(op);
            }
        }

        if (setFlags) {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRBitClearOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    const bool setFlags = BitmaskEnum(op->flags).AnyOf(arm::Flags::NZ);

    if (op->lhs.immediate && op->rhs.immediate) {
        auto result = op->lhs.imm.value & ~op->rhs.imm.value;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (setFlags) {
            const arm::Flags flagsSet = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, flagsSet);
        }
    } else {
        if (op->lhs.immediate != op->rhs.immediate) {
            if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
                auto [immValue, var] = *optPair;

                // Replace BIC by 0x00000000 with a copy of the variable
                // Replace BIC by 0xFFFFFFFF with the constant 0
                if (op->rhs.immediate && op->rhs.imm.value == 0) {
                    Assign(op->dst, var);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, var, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                } else if (op->rhs.immediate && op->rhs.imm.value == ~0) {
                    Assign(op->dst, 0);
                    if (setFlags) {
                        m_emitter.Overwrite().Move(op->dst, 0, true);
                    } else {
                        m_emitter.Erase(op);
                    }
                }
            }
        } else if (op->lhs.var == op->rhs.var) {
            // Replace BIC between the same variables with zero
            Assign(op->dst, 0);
            if (setFlags) {
                m_emitter.Overwrite().Move(op->dst, 0, true);
            } else {
                m_emitter.Erase(op);
            }
        }

        if (setFlags) {
            ClearKnownHostFlags(op->flags);
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = std::countl_zero(op->value.imm.value);
        Assign(op->dst, result);
        m_emitter.Erase(op);
    }
}

void ConstPropagationOptimizerPass::Process(IRAddOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        const arm::Flags flags = op->flags;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(result, carry, overflow);
            SetKnownHostFlags(arm::Flags::NZCV, setFlags);
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(op->flags);
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace ADD by 0 with a copy of the other variable
            if (immValue == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRAddCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    auto carryFlag = GetCarryFlag();
    if (op->lhs.immediate && op->rhs.immediate && carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::ADC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
        const arm::Flags flags = op->flags;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(result, carry, overflow);
            SetKnownHostFlags(arm::Flags::NZCV, setFlags);
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(op->flags);
    } else if (op->lhs.immediate != op->rhs.immediate && carryFlag.has_value() && !*carryFlag) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace ADC by 0 when the carry is clear with a copy of the other variable
            if (immValue == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        const arm::Flags flags = op->flags;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(result, carry, overflow);
            SetKnownHostFlags(arm::Flags::NZCV, setFlags);
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(op->flags);
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace SUB by 0 (rhs only) with a copy of the other variable
            if (op->rhs.immediate && op->rhs.imm.value == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRSubtractCarryOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    auto carryFlag = GetCarryFlag();
    if (op->lhs.immediate && op->rhs.immediate && carryFlag.has_value()) {
        auto [result, carry, overflow] = arm::SBC(op->lhs.imm.value, op->rhs.imm.value, *carryFlag);
        const arm::Flags flags = op->flags;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZCV)) {
            const arm::Flags setFlags = m_emitter.SetNZCV(result, carry, overflow);
            SetKnownHostFlags(arm::Flags::NZCV, setFlags);
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(op->flags);
    } else if (op->lhs.immediate != op->rhs.immediate && carryFlag.has_value() && *carryFlag) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace SBC by 0 (rhs only) when the carry is set with a copy of the other variable
            if (op->rhs.immediate && op->rhs.imm.value == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveOp *op) {
    Substitute(op->value);
    Assign(op->dst, op->value);
    if (op->value.immediate) {
        const uint32_t value = op->value.imm.value;
        const arm::Flags flags = op->flags;
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(value);
            SetKnownHostFlags(arm::Flags::NZ, setFlags);
        }
    } else if (BitmaskEnum(op->flags).NoneOf(arm::Flags::NZ)) {
        m_emitter.Erase(op);
    } else {
        ClearKnownHostFlags(op->flags);
    }
}

void ConstPropagationOptimizerPass::Process(IRMoveNegatedOp *op) {
    Substitute(op->value);
    if (op->value.immediate) {
        auto result = ~op->value.imm.value;
        const arm::Flags flags = op->flags;
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, setFlags);
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
        m_emitter.Erase(op);

        if (q) {
            m_emitter.StoreFlags(arm::Flags::V, static_cast<uint32_t>(arm::Flags::V));
            SetKnownHostFlags(arm::Flags::V, arm::Flags::V);
        } else {
            SetKnownHostFlags(arm::Flags::V, arm::Flags::None);
        }
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace QADD by 0 with a copy of the other variable
            if (immValue == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    } else {
        ClearKnownHostFlags(arm::Flags::V);
    }
}

void ConstPropagationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        auto [result, q] = arm::Saturate((int64_t)op->lhs.imm.value - op->rhs.imm.value);
        Assign(op->dst, result);
        m_emitter.Erase(op);

        if (q) {
            m_emitter.StoreFlags(arm::Flags::V, static_cast<uint32_t>(arm::Flags::V));
            SetKnownHostFlags(arm::Flags::V, arm::Flags::V);
        } else {
            SetKnownHostFlags(arm::Flags::V, arm::Flags::None);
        }
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace QSUB by 0 (rhs only) with a copy of the other variable
            if (op->rhs.immediate && op->rhs.imm.value == 0) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    } else {
        ClearKnownHostFlags(arm::Flags::V);
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result = (int32_t)op->lhs.imm.value * (int32_t)op->rhs.imm.value;
            const arm::Flags flags = op->flags;
            Assign(op->dst, result);
            m_emitter.Erase(op);

            if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ((uint32_t)result);
                SetKnownHostFlags(arm::Flags::NZ, setFlags);
            }
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            const arm::Flags flags = op->flags;
            Assign(op->dst, result);
            m_emitter.Erase(op);

            if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(result);
                SetKnownHostFlags(arm::Flags::NZ, setFlags);
            }
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(arm::Flags::NZ);
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            // Replace MUL by 1 with a copy of the other variable
            if (immValue == 1) {
                Assign(op->dst, var);
                m_emitter.Erase(op);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRMultiplyLongOp *op) {
    Substitute(op->lhs);
    Substitute(op->rhs);
    if (op->lhs.immediate && op->rhs.immediate) {
        if (op->signedMul) {
            auto result =
                bit::sign_extend<32, int64_t>(op->lhs.imm.value) * bit::sign_extend<32, int64_t>(op->rhs.imm.value);
            if (op->shiftDownHalf) {
                result >>= 16ll;
            }
            const arm::Flags flags = op->flags;
            Assign(op->dstLo, result >> 0ll);
            Assign(op->dstHi, result >> 32ll);
            m_emitter.Erase(op);

            if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ((uint64_t)result);
                SetKnownHostFlags(arm::Flags::NZ, setFlags);
            }
        } else {
            auto result = (uint64_t)op->lhs.imm.value * (uint64_t)op->rhs.imm.value;
            if (op->shiftDownHalf) {
                result >>= 16ull;
            }
            const arm::Flags flags = op->flags;
            Assign(op->dstLo, result >> 0ull);
            Assign(op->dstHi, result >> 32ull);
            m_emitter.Erase(op);

            if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
                const arm::Flags setFlags = m_emitter.SetNZ(result);
                SetKnownHostFlags(arm::Flags::NZ, setFlags);
            }
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(arm::Flags::NZ);
    } else if (op->lhs.immediate != op->rhs.immediate) {
        if (auto optPair = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [immValue, var] = *optPair;

            if (op->shiftDownHalf) {
                // Replace MULLH by 0x10000 with a copy of the other variable
                if (immValue == 0x10000) {
                    Assign(op->dstLo, var);
                    Assign(op->dstHi, 0);
                    m_emitter.Erase(op);
                }
            } else {
                // Replace MULL by 1 with a copy of the other variable
                if (immValue == 1) {
                    Assign(op->dstLo, var);
                    Assign(op->dstHi, 0);
                    m_emitter.Erase(op);
                }
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
        const arm::Flags flags = op->flags;
        Assign(op->dstLo, result >> 0ull);
        Assign(op->dstHi, result >> 32ull);
        m_emitter.Erase(op);

        if (BitmaskEnum(flags).AnyOf(arm::Flags::NZ)) {
            const arm::Flags setFlags = m_emitter.SetNZ(result);
            SetKnownHostFlags(arm::Flags::NZ, setFlags);
        }
    } else if (op->flags != arm::Flags::None) {
        ClearKnownHostFlags(arm::Flags::NZ);
    } else if (op->lhsLo.immediate != op->rhsLo.immediate && op->lhsHi.immediate != op->rhsHi.immediate &&
               op->lhsLo.immediate == op->lhsHi.immediate) {
        auto optPairLo = SplitImmVarPair(op->lhsLo, op->rhsLo);
        auto optPairHi = SplitImmVarPair(op->lhsHi, op->rhsHi);
        if (optPairLo && optPairHi) {
            auto [immValueLo, varLo] = *optPairLo;
            auto [immValueHi, varHi] = *optPairHi;

            // Replace ADDL by 0 with a copy of the other variable
            if (immValueLo == 0 && immValueHi == 0) {
                Assign(op->dstLo, varLo);
                Assign(op->dstHi, varHi);
                m_emitter.Erase(op);
            }
        }
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
            Assign(op->dstCPSR, static_cast<uint32_t>(cpsr));
            m_emitter.Erase(op);
        } else {
            const auto srcCPSR = op->srcCPSR;
            const auto dstCPSR = op->dstCPSR;

            if (mask == arm::Flags::None) {
                if (srcCPSR.immediate) {
                    Assign(dstCPSR, srcCPSR.imm.value);
                } else {
                    Assign(dstCPSR, srcCPSR.var);
                }
                m_emitter.Erase(op);
            } else {
                m_emitter.Overwrite();
                auto cpsr = m_emitter.BitClear(srcCPSR, static_cast<uint32_t>(mask), false);
                cpsr = m_emitter.BitwiseOr(cpsr, static_cast<uint32_t>(hostFlags), false);
                m_emitter.CopyVar(dstCPSR, cpsr);
            }
        }
    }
}

void ConstPropagationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    Substitute(op->srcCPSR);
    const arm::Flags mask = op->setQ ? arm::Flags::V : arm::Flags::None;
    if (BitmaskEnum(m_knownHostFlagsMask).AllOf(mask)) {
        auto hostFlags = m_knownHostFlagsValues & mask;
        const auto srcCPSR = op->srcCPSR;
        const auto dstCPSR = op->dstCPSR;

        if (BitmaskEnum(hostFlags).AnyOf(arm::Flags::V)) {
            auto cpsr = m_emitter.BitwiseOr(srcCPSR, (1u << 27u), false);
            Assign(dstCPSR, cpsr);
        } else if (srcCPSR.immediate) {
            Assign(dstCPSR, srcCPSR.imm.value);
        } else {
            Assign(dstCPSR, srcCPSR.var);
        }
        m_emitter.Erase(op);
    }
}

void ConstPropagationOptimizerPass::Process(IRBranchOp *op) {
    Substitute(op->address);
    Forget(arm::GPR::PC);

    // If a variable branch became an immediate branch, replace the terminal with a direct link
    if (op->address.immediate && m_emitter.GetBlock().GetTerminal() != BasicBlock::Terminal::DirectLink) {
        m_emitter.TerminateDirectLink(op->address.imm.value, m_emitter.Mode(), m_emitter.IsThumbMode());
        MarkDirty();
    }
}

void ConstPropagationOptimizerPass::Process(IRBranchExchangeOp *op) {
    Substitute(op->address);
    Forget(arm::GPR::PC);

    // If a variable branch became an immediate branch, replace the terminal with a direct link
    if (op->address.immediate && !op->bx4 && m_emitter.GetBlock().GetTerminal() != BasicBlock::Terminal::DirectLink) {
        m_emitter.TerminateDirectLink(op->address.imm.value, m_emitter.Mode(), bit::test<0>(op->address.imm.value));
        MarkDirty();
    }
}

void ConstPropagationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    Substitute(op->srcValue);
}

void ConstPropagationOptimizerPass::Process(IRConstantOp *op) {
    Assign(op->dst, op->value);
    m_emitter.Erase(op);
}

void ConstPropagationOptimizerPass::Process(IRCopyVarOp *op) {
    Substitute(op->var);
    Assign(op->dst, op->var);
    m_emitter.Erase(op);
}

// ---------------------------------------------------------------------------------------------------------------------
// Variable substitutions

void ConstPropagationOptimizerPass::ResizeVarSubsts(size_t index) {
    if (m_varSubsts.size() <= index) {
        m_varSubsts.resize(index + 1);
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

// ---------------------------------------------------------------------------------------------------------------------
// GPR substitutions

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
    return m_gprSubsts[gpr.Index()];
}

// ---------------------------------------------------------------------------------------------------------------------
// Host flag state tracking

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

// ---------------------------------------------------------------------------------------------------------------------
// Value substitution functions

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
