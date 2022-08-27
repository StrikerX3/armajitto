#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "armajitto/guest/arm/arithmetic.hpp"
#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"
#include "armajitto/util/bit_ops.hpp"
#include "armajitto/util/pointer_cast.hpp"

#include "abi.hpp"
#include "vtune.hpp"
#include "x86_64_compiler.hpp"

#include <cstdio>

namespace armajitto::x86_64 {

// FIXME: remove this code; this is just to get things going
Xbyak::Allocator g_alloc;
void *ptr = g_alloc.alloc(4096);
Xbyak::CodeGenerator code{4096, ptr, &g_alloc};
// FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME

// ---------------------------------------------------------------------------------------------------------------------

// From Dynarmic:

// This is a constant used to create the x64 flags format from the ARM format.
// NZCV * multiplier: NZCV0NZCV000NZCV
// x64_flags format:  NZ-----C-------V
constexpr uint32_t ARMTox64FlagsMultiplier = 0x1081;

// This is a constant used to create the ARM format from the x64 flags format.
constexpr uint32_t x64ToARMFlagsMultiplier = 0x1021'0000;

constexpr uint32_t ARMflgNPos = 31u;
constexpr uint32_t ARMflgZPos = 30u;
constexpr uint32_t ARMflgCPos = 29u;
constexpr uint32_t ARMflgVPos = 28u;
constexpr uint32_t ARMflgQPos = 27u;
constexpr uint32_t ARMflgNZCVShift = 28u;

constexpr uint32_t ARMflgN = (1u << ARMflgNPos);
constexpr uint32_t ARMflgZ = (1u << ARMflgZPos);
constexpr uint32_t ARMflgC = (1u << ARMflgCPos);
constexpr uint32_t ARMflgV = (1u << ARMflgVPos);
constexpr uint32_t ARMflgQ = (1u << ARMflgQPos);

constexpr uint32_t x64flgNPos = 15u;
constexpr uint32_t x64flgZPos = 14u;
constexpr uint32_t x64flgCPos = 8u;
constexpr uint32_t x64flgVPos = 0u;

constexpr uint32_t x64flgN = (1u << x64flgNPos);
constexpr uint32_t x64flgZ = (1u << x64flgZPos);
constexpr uint32_t x64flgC = (1u << x64flgCPos);
constexpr uint32_t x64flgV = (1u << x64flgVPos);

constexpr uint32_t ARMFlagsMask = ARMflgN | ARMflgZ | ARMflgC | ARMflgV | ARMflgQ;
constexpr uint32_t x64FlagsMask = x64flgN | x64flgZ | x64flgC | x64flgV;

// ---------------------------------------------------------------------------------------------------------------------

x64Host::x64Host(Context &context)
    : Host(context) {

    CompileProlog();
    CompileEpilog();
}

void x64Host::Compile(ir::BasicBlock &block) {
    Compiler compiler{code};

    compiler.Analyze(block);

    auto fnPtr = code.getCurr<HostCode::Fn>();
    code.setProtectModeRW();

    // TODO: check condition code
    // TODO: update PC if condition fails

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        auto opStr = op->ToString();
        printf("  compiling '%s'\n", opStr.c_str());
        compiler.PreProcessOp(op);
        ir::VisitIROp(op, [this, &compiler](const auto *op) -> void { CompileOp(compiler, op); });
        compiler.PostProcessOp(op);
        op = op->Next();
    }

    // TODO: block linking
    // TODO: cycle counting

    // Go to epilog
    code.mov(abi::kNonvolatileRegs[0], m_epilog.GetPtr());
    code.jmp(abi::kNonvolatileRegs[0]);

    code.setProtectModeRE();
    SetHostCode(block, fnPtr);
    vtune::ReportBasicBlock(CastUintPtr(fnPtr), code.getCurr<uintptr_t>(), block.Location());
}

void x64Host::CompileProlog() {
    m_prolog = code.getCurr<PrologFn>();
    code.setProtectModeRW();

    // Push all nonvolatile registers
    for (auto &reg : abi::kNonvolatileRegs) {
        code.push(reg);
    }

    // Setup stack -- make space for register spill area
    code.sub(rsp, abi::kStackReserveSize);

    // Copy CPSR NZCV flags to ah/al
    code.mov(eax, dword[CastUintPtr(&m_armState.CPSR())]);
    code.shr(eax, ARMflgNZCVShift); // Shift NZCV bits to [3..0]
    if (CPUID::HasFastPDEPAndPEXT()) {
        // AH       AL
        // SZ0A0P1C -------V
        // NZ.....C .......V
        auto depMask = abi::kNonvolatileRegs[0];
        code.mov(depMask.cvt32(), 0b11000001'00000001u); // Deposit bit mask: NZ-----C -------V
        code.pdep(eax, eax, depMask.cvt32());
    } else {
        code.imul(eax, eax, ARMTox64FlagsMultiplier); // eax = -------- -------- NZCV-NZC V---NZCV
        code.and_(eax, x64FlagsMask);                 // eax = -------- -------- NZ-----C -------V
    }

    // Setup static registers and call block function
    auto funcAddr = abi::kNonvolatileRegs.back();
    code.mov(funcAddr, abi::kIntArgRegs[0]); // Get block code pointer from 1st arg
    code.mov(rbx, CastUintPtr(&m_armState)); // rbx = ARM state pointer
    code.jmp(funcAddr);                      // Jump to block code

    code.setProtectModeRE();
    vtune::ReportCode(CastUintPtr(m_prolog), code.getCurr<uintptr_t>(), "__prolog");
}

void x64Host::CompileEpilog() {
    m_epilog = code.getCurr<HostCode::Fn>();
    code.setProtectModeRW();

    // Cleanup stack
    code.add(rsp, abi::kStackReserveSize);

    // Pop all nonvolatile registers
    for (auto it = abi::kNonvolatileRegs.rbegin(); it != abi::kNonvolatileRegs.rend(); it++) {
        code.pop(*it);
    }

    // Return from call
    code.ret();

    code.setProtectModeRE();
    vtune::ReportCode(m_epilog.GetPtr(), code.getCurr<uintptr_t>(), "__epilog");
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetRegisterOp *op) {
    auto dstReg = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.GPROffset(op->src.gpr, op->src.Mode());
    code.mov(dstReg, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op) {
    auto offset = m_armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op) {
    auto dstReg = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.CPSROffset();
    code.mov(dstReg, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op) {
    auto offset = m_armState.CPSROffset();
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op) {
    auto dstReg = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.SPSROffset(op->mode);
    code.mov(dstReg, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op) {
    auto offset = m_armState.SPSROffset(op->mode);
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemReadOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemWriteOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRPreloadOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftLeftOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 0 of the base value.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSL(op->value.imm.value, op->amount.imm.value);
        AssignImmResult(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg = compiler.regAlloc.GetRCX();
        auto valueReg = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg, 63);
        code.cmp(amountReg, 63);
        code.cmovbe(shiftReg.cvt32(), amountReg);

        // Get destination register
        Xbyak::Reg64 dstReg{};
        if (op->dst.var.IsPresent()) {
            dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg, valueReg);
        } else {
            dstReg = compiler.regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        code.shl(dstReg, 32); // Shift value to the top half of the 64-bit register
        code.shl(dstReg, shiftReg.cvt8());
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
        code.shr(dstReg, 32); // Shift value back down to the bottom half
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg, 63);
        code.cmp(amountReg, 63);
        code.cmovbe(shiftReg.cvt32(), amountReg);

        // Get destination register
        Xbyak::Reg64 dstReg{};
        if (op->dst.var.IsPresent()) {
            dstReg = compiler.regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg = compiler.regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        code.mov(dstReg, static_cast<uint64_t>(value) << 32ull);
        code.shl(dstReg, shiftReg.cvt8());
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
        code.shr(dstReg, 32); // Shift value back down to the bottom half
    } else {
        // value is variable, amount is immediate
        auto valueReg = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg64 dstReg{};
            if (op->dst.var.IsPresent()) {
                dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg, valueReg);
            } else {
                dstReg = compiler.regAlloc.GetTemporary().cvt64();
            }

            // Compute shift and update flags
            code.shl(dstReg, amount + 32);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
            code.shr(dstReg, 32); // Shift value back down to the bottom half
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        code.bt(valueReg, 0);
                        SetCFromFlags(compiler);
                    }

                    auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg, dstReg);
                } else if (op->setCarry) {
                    code.bt(valueReg, 0);
                    SetCFromFlags(compiler);
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg, dstReg);
                }
                if (op->setCarry) {
                    SetCFromValue(false);
                }
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not -- larger amounts simply output zero.
    // For offset == 32, the carry flag is set to bit 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResult(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg = compiler.regAlloc.GetRCX();
        auto valueReg = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg, 63);
        code.cmp(amountReg, 63);
        code.cmovbe(shiftReg.cvt32(), amountReg);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg, valueReg);
            code.shr(dstReg, shiftReg.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            code.dec(shiftReg);
            code.bt(valueReg, shiftReg);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg, 63);
        code.cmp(amountReg, 63);
        code.cmovbe(shiftReg.cvt32(), amountReg);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg = compiler.regAlloc.Get(op->dst.var).cvt64();
            code.mov(dstReg, value);
            code.shr(dstReg, shiftReg.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg = compiler.regAlloc.GetTemporary().cvt64();
            code.mov(valueReg, (static_cast<uint64_t>(value) << 1ull));
            code.bt(valueReg, shiftReg);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Compute the shift
            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg, valueReg);
                code.shr(dstReg, amount);
                if (op->setCarry) {
                    SetCFromFlags(compiler);
                }
            } else if (op->setCarry) {
                code.bt(valueReg.cvt64(), amount - 1);
                SetCFromFlags(compiler);
            }
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        code.bt(valueReg, 31);
                        SetCFromFlags(compiler);
                    }

                    auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg, dstReg);
                } else if (op->setCarry) {
                    code.bt(valueReg, 31);
                    SetCFromFlags(compiler);
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg, dstReg);
                }
                if (op->setCarry) {
                    SetCFromValue(false);
                }
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op) {
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg{};

        if (op->value.immediate) {
            dstReg = compiler.regAlloc.Get(op->dst.var);
            code.mov(dstReg, op->value.imm.value);
        } else {
            auto valueReg = compiler.regAlloc.Get(op->value.var.var);
            dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg, valueReg);
        }

        code.bt(eax, x64flgCPos); // Refresh carry flag
        code.rcr(dstReg, 1);      // Perform RRX

        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg = compiler.regAlloc.Get(op->value.var.var);
            code.bt(valueReg, 0);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseAndOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & op->rhs.imm.value;
        AssignImmResult(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, var);

                CopyIfDifferent(dstReg, varReg);
                code.and_(dstReg, imm);
            } else if (setFlags) {
                code.test(varReg, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg = compiler.regAlloc.Get(op->dst.var);

                if (dstReg == lhsReg) {
                    code.and_(dstReg, rhsReg);
                } else if (dstReg == rhsReg) {
                    code.and_(dstReg, lhsReg);
                } else {
                    code.mov(dstReg, lhsReg);
                    code.and_(dstReg, rhsReg);
                }
            } else if (setFlags) {
                code.test(lhsReg, rhsReg);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseOrOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value | op->rhs.imm.value;
        AssignImmResult(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, var);

                CopyIfDifferent(dstReg, varReg);
                code.or_(dstReg, imm);
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, varReg);
                code.or_(tmpReg, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg = compiler.regAlloc.Get(op->dst.var);

                if (dstReg == lhsReg) {
                    code.or_(dstReg, rhsReg);
                } else if (dstReg == rhsReg) {
                    code.or_(dstReg, lhsReg);
                } else {
                    code.mov(dstReg, lhsReg);
                    code.or_(dstReg, rhsReg);
                }
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, lhsReg);
                code.or_(tmpReg, rhsReg);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseXorOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value ^ op->rhs.imm.value;
        AssignImmResult(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, var);

                CopyIfDifferent(dstReg, varReg);
                code.xor_(dstReg, imm);
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, varReg);
                code.xor_(tmpReg, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg = compiler.regAlloc.Get(op->dst.var);

                if (dstReg == lhsReg) {
                    code.xor_(dstReg, rhsReg);
                } else if (dstReg == rhsReg) {
                    code.xor_(dstReg, lhsReg);
                } else {
                    code.mov(dstReg, lhsReg);
                    code.xor_(dstReg, rhsReg);
                }
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, lhsReg);
                code.xor_(tmpReg, rhsReg);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitClearOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const uint32_t result = op->lhs.imm.value & ~op->rhs.imm.value;
        AssignImmResult(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!rhsImm) {
            // lhs is var or imm, rhs is variable
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->rhs.var.var);

                CopyIfDifferent(dstReg, rhsReg);
                code.not_(dstReg);

                if (lhsImm) {
                    code.and_(dstReg, op->lhs.imm.value);
                } else {
                    auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
                    code.and_(dstReg, lhsReg);
                }
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, rhsReg);
                code.not_(tmpReg);

                if (lhsImm) {
                    code.test(tmpReg, op->lhs.imm.value);
                } else {
                    auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
                    code.test(tmpReg, lhsReg);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);

                CopyIfDifferent(dstReg, lhsReg);
                code.and_(dstReg, ~op->rhs.imm.value);
            } else if (setFlags) {
                code.test(lhsReg, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResult(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, var);

                CopyIfDifferent(dstReg, varReg);
                code.add(dstReg, imm);
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, varReg);
                code.add(tmpReg, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg = compiler.regAlloc.Get(op->dst.var);

                if (dstReg == lhsReg) {
                    code.add(dstReg, rhsReg);
                } else if (dstReg == rhsReg) {
                    code.add(dstReg, lhsReg);
                } else {
                    code.mov(dstReg, lhsReg);
                    code.add(dstReg, rhsReg);
                }
            } else if (setFlags) {
                auto tmpReg = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg, lhsReg);
                code.add(tmpReg, rhsReg);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResult(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg = compiler.regAlloc.Get(op->lhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg, lhsReg);

                if (rhsImm) {
                    code.sub(dstReg, op->rhs.imm.value);
                } else {
                    auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);
                    code.sub(dstReg, rhsReg);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    code.cmp(lhsReg, op->rhs.imm.value);
                } else {
                    auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);
                    code.cmp(lhsReg, rhsReg);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg = compiler.regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg = compiler.regAlloc.Get(op->dst.var);
                code.mov(dstReg, op->lhs.imm.value);
                code.sub(dstReg, rhsReg);
            } else if (setFlags) {
                auto lhsReg = compiler.regAlloc.GetTemporary();
                code.mov(lhsReg, op->lhs.imm.value);
                code.cmp(lhsReg, rhsReg);
            }
        }

        if (setFlags) {
            code.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg, op->value.imm.value);
        } else {
            auto valReg = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg, valReg);
        }

        if (setFlags) {
            auto dstReg = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(op->value.imm.value);
        } else {
            auto valueReg = compiler.regAlloc.Get(op->value.var.var);
            SetNZFromReg(compiler, valueReg);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg, ~op->value.imm.value);
        } else {
            auto valReg = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(dstReg, valReg);
            code.not_(dstReg);
        }

        if (setFlags) {
            auto dstReg = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(~op->value.imm.value);
        } else {
            auto valueReg = compiler.regAlloc.Get(op->value.var.var);
            auto tmpReg = compiler.regAlloc.GetTemporary();
            code.mov(tmpReg, valueReg);
            code.not_(tmpReg);
            SetNZFromReg(compiler, tmpReg);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingAddOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingSubtractOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyLongOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddLongOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreFlagsOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadFlagsOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg = compiler.regAlloc.Get(op->dstCPSR.var);
        code.mov(dstReg, op->srcCPSR.imm.value);
    } else {
        auto srcReg = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg, srcReg);
    }

    // Apply flags to dstReg
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg = compiler.regAlloc.Get(op->dstCPSR.var);

        // Extract the host flags we need from EAX into flags
        auto flags = compiler.regAlloc.GetTemporary();
        if (CPUID::HasFastPDEPAndPEXT()) {
            code.mov(flags, x64FlagsMask);
            code.pext(flags, eax, flags);
            code.shl(flags, 28);
        } else {
            code.imul(flags, eax, x64ToARMFlagsMultiplier);
            code.and_(flags, ARMFlagsMask);
        }
        code.and_(flags, cpsrMask);   // Keep only the affected bits
        code.and_(dstReg, ~cpsrMask); // Clear affected bits from dst value
        code.or_(dstReg, flags);      // Store new bits into dst value
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg = compiler.regAlloc.Get(op->dstCPSR.var);
        code.mov(dstReg, op->srcCPSR.imm.value);
    } else {
        auto srcReg = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg, srcReg);
    }

    // Apply overflow flag to dstReg in the Q position
    if (op->setQ) {
        const uint32_t cpsrMask = ARMflgQ;
        auto dstReg = compiler.regAlloc.Get(op->dstCPSR.var);

        // Extract the overflow from EAX into flags
        auto flags = compiler.regAlloc.GetTemporary();
        code.movzx(flags, al);        // Get host overflow flag
        code.shl(flags, ARMflgQPos);  // Move flag into the Q position
        code.and_(flags, cpsrMask);   // Keep only the affected bits
        code.and_(dstReg, ~cpsrMask); // Clear affected bits from dst value
        code.or_(dstReg, flags);      // Store new bits into dst value
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRConstantOp *op) {
    if (op->dst.var.IsPresent()) {
        auto dstReg = compiler.regAlloc.Get(op->dst.var);
        code.mov(dstReg, op->value);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op) {
    compiler.regAlloc.Reuse(op->dst.var, op->var.var);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetBaseVectorAddressOp *op) {}

// ---------------------------------------------------------------------------------------------------------------------

void x64Host::SetCFromValue(bool carry) {
    if (carry) {
        code.or_(eax, x64flgC);
    } else {
        code.and_(eax, ~x64flgC);
    }
}

void x64Host::SetCFromFlags(Compiler &compiler) {
    auto tmp = compiler.regAlloc.GetTemporary();
    code.setc(tmp.cvt8());       // Put new C into a temporary register
    code.movzx(tmp, tmp.cvt8()); // Zero-extend to 32 bits
    code.shl(tmp, x64flgCPos);   // Move it to the correct position
    code.and_(eax, ~x64flgC);    // Clear existing C flag from EAX
    code.or_(eax, tmp);          // Write new C flag into EAX
}

void x64Host::SetNZFromValue(uint32_t value) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ);
    if (ones != 0) {
        code.or_(eax, ones);
    }
    if (zeros != 0) {
        code.and_(eax, ~zeros);
    }
}

void x64Host::SetNZFromReg(Compiler &compiler, Xbyak::Reg32 value) {
    auto tmp = compiler.regAlloc.GetTemporary();
    code.test(value, value); // Updates NZ, clears CV; V won't be changed here
    code.mov(tmp, eax);      // Copy current flags to preserve C later
    code.lahf();             // Load NZC; C is 0
    code.and_(tmp, x64flgC); // Keep previous C only
    code.or_(eax, tmp);      // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZFromFlags(Compiler &compiler) {
    auto tmp = compiler.regAlloc.GetTemporary();
    code.clc();              // Clear C to make way for the previous C
    code.mov(tmp, eax);      // Copy current flags to preserve C later
    code.lahf();             // Load NZC; C is 0
    code.and_(tmp, x64flgC); // Keep previous C only
    code.or_(eax, tmp);      // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZCVFromValue(uint32_t value, bool carry, bool overflow) {
    const bool n = (value >> 31u);
    const bool z = (value == 0);
    const uint32_t ones = (n * x64flgN) | (z * x64flgZ) | (carry * x64flgC);
    const uint32_t zeros = (!n * x64flgN) | (!z * x64flgZ) | (!carry * x64flgC);
    if (ones != 0) {
        code.or_(eax, ones);
    }
    if (zeros != 0) {
        code.and_(eax, ~zeros);
    }
    code.mov(al, static_cast<uint8_t>(overflow));
}

void x64Host::SetNZCVFromFlags() {
    code.lahf();
    code.seto(al);
}

void x64Host::MOVImmediate(Xbyak::Reg32 reg, uint32_t value) {
    if (value == 0) {
        code.xor_(reg, reg);
    } else {
        code.mov(reg, value);
    }
}

void x64Host::CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src) {
    if (dst != src) {
        code.mov(dst, src);
    }
}

void x64Host::CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src) {
    if (dst != src) {
        code.mov(dst, src);
    }
}

void x64Host::AssignImmResult(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg, result);
    }

    if (setFlags) {
        SetNZFromValue(result);
    }
}

void x64Host::AssignImmResult(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                              std::optional<bool> carry, bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg, result);
    }

    if (setCarry && carry) {
        SetCFromValue(*carry);
    }
}

void x64Host::AssignImmResult(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool carry,
                              bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg, result);
    }

    if (setFlags) {
        SetNZCVFromValue(result, carry, overflow);
    }
}

} // namespace armajitto::x86_64
