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
#include <limits>

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
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.GPROffset(op->src.gpr, op->src.Mode());
    code.mov(dstReg32, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op) {
    auto offset = m_armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op) {
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.CPSROffset();
    code.mov(dstReg32, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op) {
    auto offset = m_armState.CPSROffset();
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op) {
    auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
    auto offset = m_armState.SPSROffset(op->mode);
    code.mov(dstReg32, dword[rbx + offset]);
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op) {
    auto offset = m_armState.SPSROffset(op->mode);
    if (op->src.immediate) {
        code.mov(dword[rbx + offset], op->src.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->src.var.var);
        code.mov(dword[rbx + offset], srcReg32);
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
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto valueReg64 = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg64, 63);
        code.cmp(amountReg32, 63);
        code.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            code.shlx(dstReg64, valueReg64, shiftReg64);
        } else {
            Xbyak::Reg64 dstReg{};
            if (op->dst.var.IsPresent()) {
                dstReg = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg, valueReg64);
            } else {
                dstReg = compiler.regAlloc.GetTemporary().cvt64();
            }

            // Compute the shift
            code.shl(dstReg, 32); // Shift value to the top half of the 64-bit register
            code.shl(dstReg, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
            code.shr(dstReg, 32); // Shift value back down to the bottom half
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg64, 63);
        code.cmp(amountReg32, 63);
        code.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Get destination register
        Xbyak::Reg64 dstReg64{};
        if (op->dst.var.IsPresent()) {
            dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
        } else {
            dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
        }

        // Compute the shift
        code.mov(dstReg64, static_cast<uint64_t>(value) << 32ull);
        code.shl(dstReg64, shiftReg64.cvt8());
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
        code.shr(dstReg64, 32); // Shift value back down to the bottom half
    } else {
        // value is variable, amount is immediate
        auto valueReg64 = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Get destination register
            Xbyak::Reg64 dstReg64{};
            if (op->dst.var.IsPresent()) {
                dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
                CopyIfDifferent(dstReg64, valueReg64);
            } else {
                dstReg64 = compiler.regAlloc.GetTemporary().cvt64();
            }

            // Compute shift and update flags
            code.shl(dstReg64, amount + 32);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
            code.shr(dstReg64, 32); // Shift value back down to the bottom half
        } else {
            if (amount == 32) {
                if (op->dst.var.IsPresent()) {
                    // Update carry flag before zeroing out the register
                    if (op->setCarry) {
                        code.bt(valueReg64, 0);
                        SetCFromFlags(compiler);
                    }

                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg32, dstReg32);
                } else if (op->setCarry) {
                    code.bt(valueReg64, 0);
                    SetCFromFlags(compiler);
                }
            } else {
                // Zero out destination
                if (op->dst.var.IsPresent()) {
                    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                    code.xor_(dstReg32, dstReg32);
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
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto valueReg64 = compiler.regAlloc.Get(op->value.var.var).cvt64();
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg64, 63);
        code.cmp(amountReg32, 63);
        code.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            code.shrx(dstReg64, valueReg64, shiftReg64);
        } else if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var).cvt64();
            CopyIfDifferent(dstReg64, valueReg64);
            code.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            code.dec(shiftReg64);
            code.bt(valueReg64, shiftReg64);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg64 = compiler.regAlloc.GetRCX();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..63
        code.mov(shiftReg64, 63);
        code.cmp(amountReg32, 63);
        code.cmovbe(shiftReg64.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg64 = compiler.regAlloc.Get(op->dst.var).cvt64();
            code.mov(dstReg64, value);
            code.shr(dstReg64, shiftReg64.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg64 = compiler.regAlloc.GetTemporary().cvt64();
            code.mov(valueReg64, (static_cast<uint64_t>(value) << 1ull));
            code.bt(valueReg64, shiftReg64);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value;

        if (amount < 32) {
            // Compute the shift
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
                code.shr(dstReg32, amount);
                if (op->setCarry) {
                    SetCFromFlags(compiler);
                }
            } else if (op->setCarry) {
                code.bt(valueReg32.cvt64(), amount - 1);
                SetCFromFlags(compiler);
            }
        } else if (amount == 32) {
            if (op->dst.var.IsPresent()) {
                // Update carry flag before zeroing out the register
                if (op->setCarry) {
                    code.bt(valueReg32, 31);
                    SetCFromFlags(compiler);
                }

                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                code.xor_(dstReg32, dstReg32);
            } else if (op->setCarry) {
                code.bt(valueReg32, 31);
                SetCFromFlags(compiler);
            }
        } else {
            // Zero out destination
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                code.xor_(dstReg32, dstReg32);
            }
            if (op->setCarry) {
                SetCFromValue(false);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // x86 masks the shift amount to 31 or 63.
    // ARM does not, though amounts larger than 31 behave exactly the same as a shift by 31.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::LSR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..31
        code.mov(shiftReg32, 31);
        code.cmp(amountReg32, 31);
        code.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            code.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            code.dec(shiftReg32);
            code.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg32 = compiler.regAlloc.GetRCX().cvt32();
        auto value = static_cast<int32_t>(op->value.imm.value);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Get shift amount, clamped to 0..31
        code.mov(shiftReg32, 31);
        code.cmp(amountReg32, 31);
        code.cmovbe(shiftReg32.cvt32(), amountReg32);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            code.mov(dstReg32, value);
            code.sar(dstReg32, shiftReg32.cvt8());
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            auto valueReg32 = compiler.regAlloc.GetTemporary();
            code.mov(valueReg32, (static_cast<uint64_t>(value) << 1ull));
            code.bt(valueReg32, shiftReg32);
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = std::min(op->amount.imm.value, 31u);

        // Compute the shift
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
            code.sar(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        } else if (op->setCarry) {
            code.bt(valueReg32.cvt64(), amount - 1);
            SetCFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op) {
    const bool valueImm = op->value.immediate;
    const bool amountImm = op->amount.immediate;

    // ARM ROR works exactly the same as x86 ROR, including carry flag behavior.

    if (valueImm && amountImm) {
        // Both are immediates
        auto [result, carry] = arm::ROR(op->value.imm.value, op->amount.imm.value);
        AssignImmResultWithCarry(compiler, op->dst, result, carry, op->setCarry);
    } else if (!valueImm && !amountImm) {
        // Both are variables
        auto shiftReg8 = compiler.regAlloc.GetRCX().cvt8();
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Put shift amount into CL
        code.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        } else {
            dstReg32 = compiler.regAlloc.GetTemporary();
            code.mov(dstReg32, valueReg32);
        }

        // Compute the shift
        code.ror(dstReg32, shiftReg8);
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (valueImm) {
        // value is immediate, amount is variable
        auto shiftReg8 = compiler.regAlloc.GetRCX().cvt8();
        auto value = op->value.imm.value;
        auto amountReg32 = compiler.regAlloc.Get(op->amount.var.var);

        // Put shift amount into CL
        code.mov(shiftReg8, amountReg32.cvt8());

        // Put value to shift into the result register
        Xbyak::Reg32 dstReg32{};
        if (op->dst.var.IsPresent()) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
        } else if (op->setCarry) {
            dstReg32 = compiler.regAlloc.GetTemporary();
        }

        // Compute the shift
        code.mov(dstReg32, value);
        code.ror(dstReg32, shiftReg8);
        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else {
        // value is variable, amount is immediate
        auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
        auto amount = op->amount.imm.value & 31;

        if (CPUID::HasBMI2() && op->dst.var.IsPresent() && !op->setCarry) {
            // Compute the shift directly into the result register
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            code.rorx(dstReg32, valueReg32, amount);
        } else {
            // Put value to shift into the result register
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
                CopyIfDifferent(dstReg32, valueReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
                code.mov(dstReg32, valueReg32);
            }

            // Compute the shift
            code.ror(dstReg32, amount);
            if (op->setCarry) {
                SetCFromFlags(compiler);
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op) {
    // ARM RRX works exactly the same as x86 RRX, including carry flag behavior.

    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 dstReg32{};

        if (op->value.immediate) {
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            code.mov(dstReg32, op->value.imm.value);
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valueReg32);
        }

        code.bt(eax, x64flgCPos); // Refresh carry flag
        code.rcr(dstReg32, 1);    // Perform RRX

        if (op->setCarry) {
            SetCFromFlags(compiler);
        }
    } else if (op->setCarry) {
        if (op->value.immediate) {
            SetCFromValue(bit::test<0>(op->value.imm.value));
        } else {
            auto valueReg32 = compiler.regAlloc.Get(op->value.var.var);
            code.bt(valueReg32, 0);
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
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.and_(dstReg32, imm);
            } else if (setFlags) {
                code.test(varReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    code.and_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    code.and_(dstReg32, lhsReg32);
                } else {
                    code.mov(dstReg32, lhsReg32);
                    code.and_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                code.test(lhsReg32, rhsReg32);
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
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.or_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.or_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    code.or_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    code.or_(dstReg32, lhsReg32);
                } else {
                    code.mov(dstReg32, lhsReg32);
                    code.or_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg32, lhsReg32);
                code.or_(tmpReg32, rhsReg32);
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
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.xor_(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.xor_(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    code.xor_(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    code.xor_(dstReg32, lhsReg32);
                } else {
                    code.mov(dstReg32, lhsReg32);
                    code.xor_(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg32, lhsReg32);
                code.xor_(tmpReg32, rhsReg32);
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
        AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!rhsImm) {
            // lhs is var or imm, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->rhs.var.var);
                CopyIfDifferent(dstReg32, rhsReg32);
                code.not_(dstReg32);

                if (lhsImm) {
                    code.and_(dstReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    code.and_(dstReg32, lhsReg32);
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, rhsReg32);
                code.not_(tmpReg32);

                if (lhsImm) {
                    code.test(tmpReg32, op->lhs.imm.value);
                } else {
                    auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
                    code.test(tmpReg32, lhsReg32);
                }
            }
        } else {
            // lhs is variable, rhs is immediate
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
                code.and_(dstReg32, ~op->rhs.imm.value);
            } else if (setFlags) {
                code.test(lhsReg32, ~op->rhs.imm.value);
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op) {
    if (op->dst.var.IsPresent()) {
        Xbyak::Reg32 valReg32{};
        Xbyak::Reg32 dstReg32{};
        if (op->value.immediate) {
            valReg32 = compiler.regAlloc.GetTemporary();
            dstReg32 = compiler.regAlloc.Get(op->dst.var);
            code.mov(valReg32, op->value.imm.value);
        } else {
            valReg32 = compiler.regAlloc.Get(op->value.var.var);
            dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (CPUID::HasLZCNT()) {
            code.lzcnt(dstReg32, valReg32);
        } else {
            // BSR unhelpfully returns the bit offset from the right, not left
            auto valIfZero32 = compiler.regAlloc.GetTemporary();
            code.mov(valIfZero32, 0xFFFFFFFF);
            code.bsr(dstReg32, valReg32);
            code.cmovz(dstReg32, valIfZero32);
            code.neg(dstReg32);
            code.add(dstReg32, 31);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::ADD(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.add(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.add(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    code.mov(dstReg32, lhsReg32);
                }
                code.add(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg32, lhsReg32);
                code.add(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            code.mov(dstReg32, op->lhs.imm.value);

            code.bt(eax, x64flgCPos); // Load carry flag
            code.adc(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                SetNZCVFromFlags();
            }
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.bt(eax, x64flgCPos); // Load carry flag
                code.adc(dstReg32, imm);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.bt(eax, x64flgCPos); // Load carry flag
                code.adc(tmpReg32, imm);
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                code.bt(eax, x64flgCPos); // Load carry flag

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    code.mov(dstReg32, lhsReg32);
                }
                code.adc(dstReg32, op2Reg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg32, lhsReg32);
                code.bt(eax, x64flgCPos); // Load carry flag
                code.adc(tmpReg32, rhsReg32);
            }
        }

        if (setFlags) {
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        auto [result, carry, overflow] = arm::SUB(op->lhs.imm.value, op->rhs.imm.value);
        AssignImmResultWithNZCV(compiler, op->dst, result, carry, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);

                if (rhsImm) {
                    code.sub(dstReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    code.sub(dstReg32, rhsReg32);
                }
            } else if (setFlags) {
                if (rhsImm) {
                    code.cmp(lhsReg32, op->rhs.imm.value);
                } else {
                    auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                    code.cmp(lhsReg32, rhsReg32);
                }
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
                code.mov(dstReg32, op->lhs.imm.value);
                code.sub(dstReg32, rhsReg32);
            } else if (setFlags) {
                auto lhsReg32 = compiler.regAlloc.GetTemporary();
                code.mov(lhsReg32, op->lhs.imm.value);
                code.cmp(lhsReg32, rhsReg32);
            }
        }

        if (setFlags) {
            code.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    // Note: x86 and ARM have inverted borrow bits

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->dst.var.IsPresent()) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            code.mov(dstReg32, op->lhs.imm.value);

            code.bt(eax, x64flgCPos); // Load carry flag
            code.cmc();               // Complement it
            code.sbb(dstReg32, op->rhs.imm.value);
            if (setFlags) {
                code.cmc(); // x86 carry is inverted compared to ARM carry in subtractions
                SetNZCVFromFlags();
            }
        }
    } else {
        // At least one of the operands is a variable
        if (!lhsImm) {
            // lhs is variable, rhs is var or imm
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);

            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->lhs.var.var);
                CopyIfDifferent(dstReg32, lhsReg32);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }

            code.bt(eax, x64flgCPos); // Load carry flag
            code.cmc();               // Complement it
            if (rhsImm) {
                code.sbb(dstReg32, op->rhs.imm.value);
            } else {
                auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
                code.sbb(dstReg32, rhsReg32);
            }
        } else {
            // lhs is immediate, rhs is variable
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);
            Xbyak::Reg32 dstReg32{};
            if (op->dst.var.IsPresent()) {
                dstReg32 = compiler.regAlloc.Get(op->dst.var);
            } else {
                dstReg32 = compiler.regAlloc.GetTemporary();
            }
            code.mov(dstReg32, op->lhs.imm.value);
            code.bt(eax, x64flgCPos); // Load carry flag
            code.cmc();               // Complement it
            code.sbb(dstReg32, rhsReg32);
        }

        if (setFlags) {
            code.cmc(); // Complement carry output
            SetNZCVFromFlags();
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);
            CopyIfDifferent(dstReg32, valReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            SetNZFromReg(compiler, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op) {
    const bool setFlags = BitmaskEnum(op->flags).Any();
    if (op->dst.var.IsPresent()) {
        if (op->value.immediate) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            MOVImmediate(dstReg32, ~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->value.var.var);

            CopyIfDifferent(dstReg32, valReg32);
            code.not_(dstReg32);
        }

        if (setFlags) {
            auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
            SetNZFromReg(compiler, dstReg32);
        }
    } else if (setFlags) {
        if (op->value.immediate) {
            SetNZFromValue(~op->value.imm.value);
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->value.var.var);
            auto tmpReg32 = compiler.regAlloc.GetTemporary();
            code.mov(tmpReg32, valReg32);
            code.not_(tmpReg32);
            SetNZFromReg(compiler, tmpReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingAddOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal + rhsVal);
        AssignImmResultWithOverflow(compiler, op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.add(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                code.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                code.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.add(tmpReg32, imm);
                SetVFromFlags();
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    code.add(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    code.add(dstReg32, lhsReg32);
                } else {
                    code.mov(dstReg32, lhsReg32);
                    code.add(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                code.mov(maxValReg32, std::numeric_limits<int32_t>::max());
                code.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, lhsReg32);
                code.add(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSaturatingSubtractOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        const int64_t lhsVal = bit::sign_extend<32, int64_t>(op->lhs.imm.value);
        const int64_t rhsVal = bit::sign_extend<32, int64_t>(op->rhs.imm.value);
        const auto [result, overflow] = arm::Saturate(lhsVal - rhsVal);
        AssignImmResultWithOverflow(compiler, op->dst, result, overflow, setFlags);
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                code.sub(dstReg32, imm);
                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                code.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                code.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                code.sub(tmpReg32, imm);
                SetVFromFlags();
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                if (dstReg32 == lhsReg32) {
                    code.sub(dstReg32, rhsReg32);
                } else if (dstReg32 == rhsReg32) {
                    code.sub(dstReg32, lhsReg32);
                } else {
                    code.mov(dstReg32, lhsReg32);
                    code.sub(dstReg32, rhsReg32);
                }

                if (setFlags) {
                    SetVFromFlags();
                }

                // Clamp on overflow
                auto maxValReg32 = compiler.regAlloc.GetTemporary();
                code.mov(maxValReg32, std::numeric_limits<int32_t>::min());
                code.cmovo(dstReg32, maxValReg32);
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, lhsReg32);
                code.sub(tmpReg32, rhsReg32);
                SetVFromFlags();
            }
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyOp *op) {
    const bool lhsImm = op->lhs.immediate;
    const bool rhsImm = op->rhs.immediate;
    const bool setFlags = BitmaskEnum(op->flags).Any();

    if (lhsImm && rhsImm) {
        // Both are immediates
        if (op->signedMul) {
            auto result = static_cast<int32_t>(op->lhs.imm.value) * static_cast<int32_t>(op->rhs.imm.value);
            AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
        } else {
            auto result = op->lhs.imm.value * op->rhs.imm.value;
            AssignImmResultWithNZ(compiler, op->dst, result, setFlags);
        }
    } else {
        // At least one of the operands is a variable
        if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
            auto [imm, var] = *split;
            auto varReg32 = compiler.regAlloc.Get(var);

            if (op->dst.var.IsPresent()) {
                auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, var);
                CopyIfDifferent(dstReg32, varReg32);
                if (op->signedMul) {
                    code.imul(dstReg32, dstReg32, static_cast<int32_t>(imm));
                } else {
                    code.imul(dstReg32.cvt64(), dstReg32.cvt64(), imm);
                }
                if (setFlags) {
                    code.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();
                code.mov(tmpReg32, varReg32);
                if (op->signedMul) {
                    code.imul(tmpReg32, tmpReg32, static_cast<int32_t>(imm));
                } else {
                    code.imul(tmpReg32.cvt64(), tmpReg32.cvt64(), imm);
                }
                code.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        } else {
            // lhs and rhs are vars
            auto lhsReg32 = compiler.regAlloc.Get(op->lhs.var.var);
            auto rhsReg32 = compiler.regAlloc.Get(op->rhs.var.var);

            if (op->dst.var.IsPresent()) {
                compiler.regAlloc.Reuse(op->dst.var, op->lhs.var.var);
                compiler.regAlloc.Reuse(op->dst.var, op->rhs.var.var);
                auto dstReg32 = compiler.regAlloc.Get(op->dst.var);

                auto op2Reg32 = (dstReg32 == rhsReg32) ? lhsReg32 : rhsReg32;
                if (dstReg32 != lhsReg32 && dstReg32 != rhsReg32) {
                    code.mov(dstReg32, lhsReg32);
                }
                if (op->signedMul) {
                    code.imul(dstReg32, op2Reg32);
                } else {
                    code.imul(dstReg32.cvt64(), op2Reg32.cvt64());
                }
                if (setFlags) {
                    code.test(dstReg32, dstReg32); // We need NZ, but IMUL trashes both flags
                }
            } else if (setFlags) {
                auto tmpReg32 = compiler.regAlloc.GetTemporary();

                code.mov(tmpReg32, lhsReg32);
                if (op->signedMul) {
                    code.imul(tmpReg32, rhsReg32);
                } else {
                    code.imul(tmpReg32.cvt64(), rhsReg32.cvt64());
                }
                code.test(tmpReg32, tmpReg32); // We need NZ, but IMUL trashes both flags
            }
        }

        if (setFlags) {
            SetNZFromFlags(compiler);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMultiplyLongOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddLongOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreFlagsOp *op) {
    if (op->flags != arm::Flags::None) {
        const auto mask = static_cast<uint32_t>(op->flags) >> ARMflgNZCVShift;
        if (op->values.immediate) {
            const auto value = op->values.imm.value >> ARMflgNZCVShift;
            const auto ones = ((value & mask) * ARMTox64FlagsMultiplier) & x64FlagsMask;
            const auto zeros = ((~value & mask) * ARMTox64FlagsMultiplier) & x64FlagsMask;
            if (ones != 0) {
                code.or_(eax, ones);
            }
            if (zeros != 0) {
                code.and_(eax, ~zeros);
            }
        } else {
            auto valReg32 = compiler.regAlloc.Get(op->values.var.var);
            auto maskReg32 = compiler.regAlloc.GetTemporary();
            code.shr(valReg32, ARMflgNZCVShift);
            code.imul(valReg32, valReg32, ARMTox64FlagsMultiplier);
            code.and_(valReg32, x64FlagsMask);
            code.mov(maskReg32, (~mask * ARMTox64FlagsMultiplier) & x64FlagsMask);
            code.and_(eax, maskReg32);
            code.or_(eax, valReg32);
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadFlagsOp *op) {
    // Get value from srcCPSR and copy to dstCPSR, or reuse register from srcCPSR if possible
    if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        code.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }

    // Apply flags to dstReg32
    if (BitmaskEnum(op->flags).Any()) {
        const uint32_t cpsrMask = static_cast<uint32_t>(op->flags);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

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
        code.and_(flags, cpsrMask);     // Keep only the affected bits
        code.and_(dstReg32, ~cpsrMask); // Clear affected bits from dst value
        code.or_(dstReg32, flags);      // Store new bits into dst value
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);

        // Apply overflow flag in the Q position
        code.mov(dstReg32, al);         // Copy overflow flag into destination register
        code.shl(dstReg32, ARMflgQPos); // Move Q into position
        code.or_(dstReg32, srcReg32);   // OR with srcCPSR
    } else if (op->srcCPSR.immediate) {
        auto dstReg32 = compiler.regAlloc.Get(op->dstCPSR.var);
        code.mov(dstReg32, op->srcCPSR.imm.value);
    } else {
        auto srcReg32 = compiler.regAlloc.Get(op->srcCPSR.var.var);
        auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dstCPSR.var, op->srcCPSR.var.var);
        CopyIfDifferent(dstReg32, srcReg32);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRConstantOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (op->dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(op->dst.var);
        code.mov(dstReg32, op->value);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op) {
    // This instruction should be optimized away, but here's an implementation anyway
    if (!op->var.var.IsPresent() || !op->dst.var.IsPresent()) {
        return;
    }
    auto varReg32 = compiler.regAlloc.Get(op->var.var);
    auto dstReg32 = compiler.regAlloc.ReuseAndGet(op->dst.var, op->var.var);
    CopyIfDifferent(dstReg32, varReg32);
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
    auto tmp32 = compiler.regAlloc.GetTemporary();
    code.setc(tmp32.cvt8());         // Put new C into a temporary register
    code.movzx(tmp32, tmp32.cvt8()); // Zero-extend to 32 bits
    code.shl(tmp32, x64flgCPos);     // Move it to the correct position
    code.and_(eax, ~x64flgC);        // Clear existing C flag from EAX
    code.or_(eax, tmp32);            // Write new C flag into EAX
}

void x64Host::SetVFromValue(bool overflow) {
    if (overflow) {
        code.mov(al, 1);
    } else {
        code.xor_(al, al);
    }
}

void x64Host::SetVFromFlags() {
    code.seto(al);
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
    auto tmp32 = compiler.regAlloc.GetTemporary();
    code.test(value, value);   // Updates NZ, clears CV; V won't be changed here
    code.mov(tmp32, eax);      // Copy current flags to preserve C later
    code.lahf();               // Load NZC; C is 0
    code.and_(tmp32, x64flgC); // Keep previous C only
    code.or_(eax, tmp32);      // Put previous C into AH; NZ is now updated and C is preserved
}

void x64Host::SetNZFromFlags(Compiler &compiler) {
    auto tmp32 = compiler.regAlloc.GetTemporary();
    code.clc();                // Clear C to make way for the previous C
    code.mov(tmp32, eax);      // Copy current flags to preserve C later
    code.lahf();               // Load NZC; C is 0
    code.and_(tmp32, x64flgC); // Keep previous C only
    code.or_(eax, tmp32);      // Put previous C into AH; NZ is now updated and C is preserved
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

void x64Host::AssignImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZFromValue(result);
    }
}

void x64Host::AssignImmResultWithNZCV(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool carry,
                                      bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetNZCVFromValue(result, carry, overflow);
    }
}

void x64Host::AssignImmResultWithCarry(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                       std::optional<bool> carry, bool setCarry) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setCarry && carry) {
        SetCFromValue(*carry);
    }
}

void x64Host::AssignImmResultWithOverflow(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                          bool overflow, bool setFlags) {
    if (dst.var.IsPresent()) {
        auto dstReg32 = compiler.regAlloc.Get(dst.var);
        MOVImmediate(dstReg32, result);
    }

    if (setFlags) {
        SetVFromValue(overflow);
    }
}

} // namespace armajitto::x86_64
