#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "abi.hpp"
#include "armajitto/host/x86_64/cpuid.hpp"
#include "armajitto/ir/ops/ir_ops_visitor.hpp"
#include "armajitto/util/pointer_cast.hpp"

#include <cstdio>

namespace armajitto::x86_64 {

// FIXME: remove this code; this is just to get things going
Xbyak::Allocator g_alloc;
void *ptr = g_alloc.alloc(4096);
Xbyak::CodeGenerator code{4096, ptr, &g_alloc};
// FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME

// ---------------------------------------------------------------------------------------------------------------------

x64Host::x64Host(Context &context)
    : Host(context) {

    m_prolog = CompileProlog();
    m_epilog = CompileEpilog();
}

HostCode x64Host::Compile(const ir::BasicBlock &block) {
    auto fnPtr = code.getCurr<HostCode::Fn>();
    code.setProtectModeRW();

    // TODO: check condition code

    // Compile block code
    auto *op = block.Head();
    while (op != nullptr) {
        auto opStr = op->ToString();
        printf("  compiling '%s'\n", opStr.c_str());
        ir::VisitIROp(op, [this](const auto *op) -> void { CompileOp(op); });
        op = op->Next();
    }

    // Go to epilog
    code.mov(abi::kNonvolatileRegs[0], m_epilog.GetPtr());
    code.jmp(abi::kNonvolatileRegs[0]);

    code.setProtectModeRE();
    return {fnPtr};
}

auto x64Host::CompileProlog() -> PrologFn {
    auto fnPtr = code.getCurr<PrologFn>();
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
    code.mov(eax, dword[CastUintPtr(&m_context.GetARMState().CPSR())]);
    if (CPUID::Instance().HasFastPDEPAndPEXT()) {
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
    code.mov(scratchReg, abi::kIntArgRegs[0]);            // Get block code pointer from 1st arg
    code.mov(rcx, CastUintPtr(&m_context.GetARMState())); // rcx = ARM state pointer
    code.jmp(scratchReg);                                 // Jump to block code

    code.setProtectModeRE();
    return fnPtr;
}

HostCode x64Host::CompileEpilog() {
    auto fnPtr = code.getCurr<HostCode::Fn>();
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
    return fnPtr;
}

void x64Host::CompileOp(const ir::IRGetRegisterOp *op) {}

void x64Host::CompileOp(const ir::IRSetRegisterOp *op) {
    if (op->src.immediate) {
        auto offset = m_context.GetARMState().GPROffset(op->dst.gpr, op->dst.Mode());
        code.mov(dword[rcx + offset], op->src.imm.value);
    }
}

void x64Host::CompileOp(const ir::IRGetCPSROp *op) {}

void x64Host::CompileOp(const ir::IRSetCPSROp *op) {}

void x64Host::CompileOp(const ir::IRGetSPSROp *op) {}

void x64Host::CompileOp(const ir::IRSetSPSROp *op) {}

void x64Host::CompileOp(const ir::IRMemReadOp *op) {}

void x64Host::CompileOp(const ir::IRMemWriteOp *op) {}

void x64Host::CompileOp(const ir::IRPreloadOp *op) {}

void x64Host::CompileOp(const ir::IRLogicalShiftLeftOp *op) {}

void x64Host::CompileOp(const ir::IRLogicalShiftRightOp *op) {}

void x64Host::CompileOp(const ir::IRArithmeticShiftRightOp *op) {}

void x64Host::CompileOp(const ir::IRRotateRightOp *op) {}

void x64Host::CompileOp(const ir::IRRotateRightExtendedOp *op) {}

void x64Host::CompileOp(const ir::IRBitwiseAndOp *op) {}

void x64Host::CompileOp(const ir::IRBitwiseOrOp *op) {}

void x64Host::CompileOp(const ir::IRBitwiseXorOp *op) {}

void x64Host::CompileOp(const ir::IRBitClearOp *op) {}

void x64Host::CompileOp(const ir::IRCountLeadingZerosOp *op) {}

void x64Host::CompileOp(const ir::IRAddOp *op) {}

void x64Host::CompileOp(const ir::IRAddCarryOp *op) {}

void x64Host::CompileOp(const ir::IRSubtractOp *op) {}

void x64Host::CompileOp(const ir::IRSubtractCarryOp *op) {}

void x64Host::CompileOp(const ir::IRMoveOp *op) {}

void x64Host::CompileOp(const ir::IRMoveNegatedOp *op) {}

void x64Host::CompileOp(const ir::IRSaturatingAddOp *op) {}

void x64Host::CompileOp(const ir::IRSaturatingSubtractOp *op) {}

void x64Host::CompileOp(const ir::IRMultiplyOp *op) {}

void x64Host::CompileOp(const ir::IRMultiplyLongOp *op) {}

void x64Host::CompileOp(const ir::IRAddLongOp *op) {}

void x64Host::CompileOp(const ir::IRStoreFlagsOp *op) {}

void x64Host::CompileOp(const ir::IRLoadFlagsOp *op) {}

void x64Host::CompileOp(const ir::IRLoadStickyOverflowOp *op) {}

void x64Host::CompileOp(const ir::IRBranchOp *op) {}

void x64Host::CompileOp(const ir::IRBranchExchangeOp *op) {}

void x64Host::CompileOp(const ir::IRLoadCopRegisterOp *op) {}

void x64Host::CompileOp(const ir::IRStoreCopRegisterOp *op) {}

void x64Host::CompileOp(const ir::IRConstantOp *op) {}

void x64Host::CompileOp(const ir::IRCopyVarOp *op) {}

void x64Host::CompileOp(const ir::IRGetBaseVectorAddressOp *op) {}

} // namespace armajitto::x86_64
