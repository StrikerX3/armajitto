#pragma once

#include "armajitto/host/host.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <xbyak/xbyak.h>

namespace armajitto::x86_64 {

class x64Host final : public Host {
public:
    x64Host(Context &context);
    void Compile(ir::BasicBlock &block) final;

    void Call(const ir::BasicBlock &block) {
        auto fn = GetHostCode(block).GetPtr();
        m_prolog(fn);
    }

private:
    using PrologFn = void (*)(uintptr_t blockFn);
    PrologFn m_prolog;
    HostCode m_epilog;

    struct Compiler;

    void CompileProlog();
    void CompileEpilog();

    // Catch-all method for unimplemented ops, required by the visitor
    template <typename T>
    void CompileOp(Compiler &compiler, const T *op) {}

    void CompileOp(Compiler &compiler, const ir::IRGetRegisterOp *op);
    void CompileOp(Compiler &compiler, const ir::IRSetRegisterOp *op);
    void CompileOp(Compiler &compiler, const ir::IRGetCPSROp *op);
    void CompileOp(Compiler &compiler, const ir::IRSetCPSROp *op);
    void CompileOp(Compiler &compiler, const ir::IRGetSPSROp *op);
    void CompileOp(Compiler &compiler, const ir::IRSetSPSROp *op);
    void CompileOp(Compiler &compiler, const ir::IRMemReadOp *op);
    void CompileOp(Compiler &compiler, const ir::IRMemWriteOp *op);
    void CompileOp(Compiler &compiler, const ir::IRPreloadOp *op);
    void CompileOp(Compiler &compiler, const ir::IRLogicalShiftLeftOp *op);
    void CompileOp(Compiler &compiler, const ir::IRLogicalShiftRightOp *op);
    void CompileOp(Compiler &compiler, const ir::IRArithmeticShiftRightOp *op);
    void CompileOp(Compiler &compiler, const ir::IRRotateRightOp *op);
    void CompileOp(Compiler &compiler, const ir::IRRotateRightExtendedOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBitwiseAndOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBitwiseOrOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBitwiseXorOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBitClearOp *op);
    void CompileOp(Compiler &compiler, const ir::IRCountLeadingZerosOp *op);
    void CompileOp(Compiler &compiler, const ir::IRAddOp *op);
    void CompileOp(Compiler &compiler, const ir::IRAddCarryOp *op);
    void CompileOp(Compiler &compiler, const ir::IRSubtractOp *op);
    void CompileOp(Compiler &compiler, const ir::IRSubtractCarryOp *op);
    void CompileOp(Compiler &compiler, const ir::IRMoveOp *op);
    void CompileOp(Compiler &compiler, const ir::IRMoveNegatedOp *op);
    void CompileOp(Compiler &compiler, const ir::IRSaturatingAddOp *op);
    void CompileOp(Compiler &compiler, const ir::IRSaturatingSubtractOp *op);
    void CompileOp(Compiler &compiler, const ir::IRMultiplyOp *op);
    void CompileOp(Compiler &compiler, const ir::IRMultiplyLongOp *op);
    void CompileOp(Compiler &compiler, const ir::IRAddLongOp *op);
    void CompileOp(Compiler &compiler, const ir::IRStoreFlagsOp *op);
    void CompileOp(Compiler &compiler, const ir::IRLoadFlagsOp *op);
    void CompileOp(Compiler &compiler, const ir::IRLoadStickyOverflowOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBranchOp *op);
    void CompileOp(Compiler &compiler, const ir::IRBranchExchangeOp *op);
    void CompileOp(Compiler &compiler, const ir::IRLoadCopRegisterOp *op);
    void CompileOp(Compiler &compiler, const ir::IRStoreCopRegisterOp *op);
    void CompileOp(Compiler &compiler, const ir::IRConstantOp *op);
    void CompileOp(Compiler &compiler, const ir::IRCopyVarOp *op);
    void CompileOp(Compiler &compiler, const ir::IRGetBaseVectorAddressOp *op);
};

} // namespace armajitto::x86_64