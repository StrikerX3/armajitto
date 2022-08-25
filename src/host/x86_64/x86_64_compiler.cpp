#include "armajitto/host/x86_64/x86_64_compiler.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cstdio>

namespace armajitto::x86_64 {

// FIXME: remove this code; this is just to get things going
Xbyak::Allocator g_alloc;
void *ptr = g_alloc.alloc(4096);
Xbyak::CodeGenerator code{4096, ptr, &g_alloc};
// FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME

using namespace Xbyak::util;

x64Compiler::x64Compiler(Context &context)
    : Compiler(context) {}

HostCode x64Compiler::Compile(const ir::BasicBlock &block) {
    auto *op = block.Head();

    code.mov(rcx, uintptr_t(&m_context.GetARMState()));

    while (op != nullptr) {
        auto opStr = op->ToString();
        printf("  compiling '%s'\n", opStr.c_str());
        ir::VisitIROp(op, [this](const auto *op) -> void { CompileOp(op); });
        op = op->Next();
    }

    code.ret();
    code.setProtectModeRE();
    auto *fnPtr = code.getCode<HostCode::Fn>();
    return {fnPtr};
}

void x64Compiler::CompileOp(const ir::IRGetRegisterOp *op) {}

void x64Compiler::CompileOp(const ir::IRSetRegisterOp *op) {
    if (op->src.immediate) {
        auto offset = m_context.GetARMState().GPROffset(op->dst.gpr, op->dst.Mode());
        code.mov(dword[rcx + offset], op->src.imm.value);
    }
}

void x64Compiler::CompileOp(const ir::IRGetCPSROp *op) {}

void x64Compiler::CompileOp(const ir::IRSetCPSROp *op) {}

void x64Compiler::CompileOp(const ir::IRGetSPSROp *op) {}

void x64Compiler::CompileOp(const ir::IRSetSPSROp *op) {}

void x64Compiler::CompileOp(const ir::IRMemReadOp *op) {}

void x64Compiler::CompileOp(const ir::IRMemWriteOp *op) {}

void x64Compiler::CompileOp(const ir::IRPreloadOp *op) {}

void x64Compiler::CompileOp(const ir::IRLogicalShiftLeftOp *op) {}

void x64Compiler::CompileOp(const ir::IRLogicalShiftRightOp *op) {}

void x64Compiler::CompileOp(const ir::IRArithmeticShiftRightOp *op) {}

void x64Compiler::CompileOp(const ir::IRRotateRightOp *op) {}

void x64Compiler::CompileOp(const ir::IRRotateRightExtendedOp *op) {}

void x64Compiler::CompileOp(const ir::IRBitwiseAndOp *op) {}

void x64Compiler::CompileOp(const ir::IRBitwiseOrOp *op) {}

void x64Compiler::CompileOp(const ir::IRBitwiseXorOp *op) {}

void x64Compiler::CompileOp(const ir::IRBitClearOp *op) {}

void x64Compiler::CompileOp(const ir::IRCountLeadingZerosOp *op) {}

void x64Compiler::CompileOp(const ir::IRAddOp *op) {}

void x64Compiler::CompileOp(const ir::IRAddCarryOp *op) {}

void x64Compiler::CompileOp(const ir::IRSubtractOp *op) {}

void x64Compiler::CompileOp(const ir::IRSubtractCarryOp *op) {}

void x64Compiler::CompileOp(const ir::IRMoveOp *op) {}

void x64Compiler::CompileOp(const ir::IRMoveNegatedOp *op) {}

void x64Compiler::CompileOp(const ir::IRSaturatingAddOp *op) {}

void x64Compiler::CompileOp(const ir::IRSaturatingSubtractOp *op) {}

void x64Compiler::CompileOp(const ir::IRMultiplyOp *op) {}

void x64Compiler::CompileOp(const ir::IRMultiplyLongOp *op) {}

void x64Compiler::CompileOp(const ir::IRAddLongOp *op) {}

void x64Compiler::CompileOp(const ir::IRStoreFlagsOp *op) {}

void x64Compiler::CompileOp(const ir::IRLoadFlagsOp *op) {}

void x64Compiler::CompileOp(const ir::IRLoadStickyOverflowOp *op) {}

void x64Compiler::CompileOp(const ir::IRBranchOp *op) {}

void x64Compiler::CompileOp(const ir::IRBranchExchangeOp *op) {}

void x64Compiler::CompileOp(const ir::IRLoadCopRegisterOp *op) {}

void x64Compiler::CompileOp(const ir::IRStoreCopRegisterOp *op) {}

void x64Compiler::CompileOp(const ir::IRConstantOp *op) {}

void x64Compiler::CompileOp(const ir::IRCopyVarOp *op) {}

void x64Compiler::CompileOp(const ir::IRGetBaseVectorAddressOp *op) {}

} // namespace armajitto::x86_64
