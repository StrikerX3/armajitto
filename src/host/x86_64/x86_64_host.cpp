#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "abi.hpp"
#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"
#include "armajitto/util/pointer_cast.hpp"
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
constexpr uint32_t kARMTox64FlagsMultiplier = 0x1081;

// This is a constant used to create the ARM format from the x64 flags format.
constexpr uint32_t kx64ToARMFlagsMultiplier = 0x1021'0000;

constexpr uint32_t kARMFlagsMask = 0xF000'0000;
constexpr uint32_t kx64FlagsMask = 0xC101;

// ---------------------------------------------------------------------------------------------------------------------

x64Host::x64Host(Context &context)
    : Host(context) {

    CompileProlog();
    CompileEpilog();
}

void x64Host::Compile(ir::BasicBlock &block) {
    Compiler compiler{};

    auto fnPtr = code.getCurr<HostCode::Fn>();
    code.setProtectModeRW();

    // TODO: check condition code
    // TODO: update PC if condition fails

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        auto opStr = op->ToString();
        printf("  compiling '%s'\n", opStr.c_str());
        ir::VisitIROp(op, [this, &compiler](const auto *op) -> void { CompileOp(compiler, op); });
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

    // Get scratch register for setup operations
    auto scratchReg = abi::kNonvolatileRegs[0];

    // Setup stack -- make space for register spill area
    code.sub(rsp, abi::kStackReserveSize);

    // Copy CPSR NZCV flags to ah/al
    code.mov(eax, dword[CastUintPtr(&m_armState.CPSR())]);
    if (CPUID::HasFastPDEPAndPEXT()) {
        // AH       AL
        // SZ0A0P1C -------V
        // NZ.....C .......V
        code.shr(eax, 28);                                  // Shift NZCV bits to [3..0]
        code.mov(scratchReg.cvt32(), 0b11000001'00000001u); // Deposit bit mask: NZ-----C -------V
        code.pdep(eax, eax, scratchReg.cvt32());
    } else {
        code.shr(eax, 13);       // eax = -------- -----NZC V....... ........
        code.shr(ax, 12);        // eax = -------- -----NZC -------- ----V...
        code.shr(eax, 1);        // eax = -------- ------NZ C------- -----V..
        code.shr(ah, 5);         // eax = -------- ------NZ -----C-- -----V..
        code.shr(eax, 2);        // eax = -------- -------- NZ-----C -------V
        code.or_(eax, (1 << 9)); // eax = -------- -------- NZ----1C -------V
    }

    // Setup static registers and call block function
    code.mov(scratchReg, abi::kIntArgRegs[0]); // Get block code pointer from 1st arg
    code.mov(rcx, CastUintPtr(&m_armState));   // rcx = ARM state pointer
    code.jmp(scratchReg);                      // Jump to block code

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
    if (!op->dst.var.IsPresent()) {
        // TODO: raise error: no destination variable for GetRegister
        return;
    }

    if (auto dstReg = compiler.regAlloc.Get(op->dst.var)) {
        auto offset = m_armState.GPROffset(op->src.gpr, op->src.Mode());
        code.mov(*dstReg, dword[rcx + offset]);
    } // else: invalid GetRegister op, no dst
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op) {
    auto offset = m_armState.GPROffset(op->dst.gpr, op->dst.Mode());
    if (op->src.immediate) {
        code.mov(dword[rcx + offset], op->src.imm.value);
    } else if (auto srcReg = compiler.regAlloc.Get(op->src.var.var)) {
        code.mov(dword[rcx + offset], *srcReg);
    } // else: invalid SetRegister op, no src
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        // TODO: raise error: no destination variable for GetCPSR
        return;
    }

    if (auto dstReg = compiler.regAlloc.Get(op->dst.var)) {
        auto offset = m_armState.CPSROffset();
        code.mov(*dstReg, dword[rcx + offset]);
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op) {
    auto offset = m_armState.CPSROffset();
    if (op->src.immediate) {
        code.mov(dword[rcx + offset], op->src.imm.value);
    } else if (auto srcReg = compiler.regAlloc.Get(op->src.var.var)) {
        code.mov(dword[rcx + offset], *srcReg);
    } else {
        // TODO: raise error: invalid st cpsr, no src
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemReadOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMemWriteOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRPreloadOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftLeftOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLogicalShiftRightOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseAndOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseOrOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitwiseXorOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBitClearOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveOp *op) {
    // dst is optional
    if (auto dstReg = compiler.regAlloc.Get(op->dst.var)) {
        if (op->value.immediate) {
            code.mov(*dstReg, op->value.imm.value);
        } else if (auto valueReg = compiler.regAlloc.Get(op->value.var.var)) {
            code.mov(*dstReg, *valueReg);
        } else {
            // TODO: raise error: invalid mov, no src
        }
    }

    // Update host flags if applicable
    auto bmFlags = BitmaskEnum(op->flags);
    if (bmFlags.Any()) {
        if (op->value.immediate) {
            const bool sign = (op->value.imm.value >> 31);
            const bool zero = (op->value.imm.value == 0);
            if (sign && zero) {
                code.or_(eax, (1 << 15) | (1 << 14));
            } else if (sign) {
                code.or_(eax, (1 << 15));
                code.and_(eax, ~(1 << 14));
            } else if (zero) {
                code.and_(eax, ~(1 << 15));
                code.or_(eax, (1 << 14));
            } else {
                code.or_(eax, ~((1 << 15) | (1 << 14)));
            }
        } else if (auto valueReg = compiler.regAlloc.Get(op->value.var.var)) {
            auto tmp = compiler.regAlloc.GetTemporary().cvt8();
            code.test(*valueReg, *valueReg); // Updates NZ, clears CV; V won't be changed here
            code.mov(tmp, ah);               // Copy current flags to preserve C later
            code.lahf();                     // Load NZC; C is 0
            code.and_(tmp, 1);               // Keep C
            code.or_(ah, tmp);               // Put old C into AH; NZ is now updated and C is preserved
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op) {
    // dst is optional
    if (auto dstReg = compiler.regAlloc.Get(op->dst.var)) {
        if (op->value.immediate) {
            code.mov(*dstReg, ~op->value.imm.value);
        } else if (auto valueReg = compiler.regAlloc.Get(op->value.var.var)) {
            code.mov(*dstReg, *valueReg);
            code.not_(*dstReg);
        } else {
            // TODO: raise error: invalid mvn, no src
        }
    }

    // Update host flags if applicable
    auto bmFlags = BitmaskEnum(op->flags);
    if (bmFlags.Any()) {
        if (op->value.immediate) {
            const bool sign = (op->value.imm.value >> 31);
            const bool zero = (op->value.imm.value == 0);
            if (sign && zero) {
                code.or_(eax, (1 << 15) | (1 << 14));
            } else if (sign) {
                code.or_(eax, (1 << 15));
                code.and_(eax, ~(1 << 14));
            } else if (zero) {
                code.and_(eax, ~(1 << 15));
                code.or_(eax, (1 << 14));
            } else {
                code.or_(eax, ~((1 << 15) | (1 << 14)));
            }
        } else if (auto valueReg = compiler.regAlloc.Get(op->value.var.var)) {
            auto tmp = compiler.regAlloc.GetTemporary().cvt8();
            code.test(*valueReg, *valueReg); // Updates NZ, clears CV; V won't be changed here
            code.mov(tmp, ah);               // Copy current flags to preserve C later
            code.lahf();                     // Load NZC; C is 0
            code.and_(tmp, 1);               // Keep C
            code.or_(ah, tmp);               // Put old C into AH; NZ is now updated and C is preserved
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
    if (auto dstReg = compiler.regAlloc.Get(op->dstCPSR.var)) {
        if (op->srcCPSR.immediate) {
            code.mov(*dstReg, op->srcCPSR.imm.value);
        } else if (auto srcReg = compiler.regAlloc.Get(op->srcCPSR.var.var)) {
            code.mov(*dstReg, *srcReg);
        } else {
            // TODO: raise error: invalid ldflg, no srcCPSR
            return;
        }

        // Apply flags to dstReg
        auto bmFlags = BitmaskEnum(op->flags);
        if (bmFlags.Any()) {
            uint32_t cpsrMask = static_cast<uint32_t>(op->flags);

            // Extract the host flags we need from EAX into flags
            auto flags = compiler.regAlloc.GetTemporary();
            if (CPUID::HasFastPDEPAndPEXT()) {
                auto flagBits = compiler.regAlloc.GetTemporary();
                code.mov(flagBits, kx64FlagsMask);
                code.pext(flags, eax, flagBits);
                code.shl(flags, 28);
            } else {
                code.imul(flags, eax, kx64ToARMFlagsMultiplier);
                code.and_(flags, kARMFlagsMask);
            }
            code.and_(flags, cpsrMask);    // Keep only the affected bits
            code.and_(*dstReg, ~cpsrMask); // Clear affected bits from dst value
            code.or_(*dstReg, flags);      // Store new bits into dst value
        }
    }
}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRLoadCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRStoreCopRegisterOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRConstantOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op) {}

void x64Host::CompileOp(Compiler &compiler, const ir::IRGetBaseVectorAddressOp *op) {}

} // namespace armajitto::x86_64
