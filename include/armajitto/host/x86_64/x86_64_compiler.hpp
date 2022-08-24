#pragma once

#include "armajitto/host/compiler.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <xbyak/xbyak.h>

namespace armajitto::x86_64 {

class x64Compiler final : public Compiler {
public:
    HostCode Compile(const ir::BasicBlock &block) final;

private:
    // Catch-all method for unimplemented ops, required by the visitor
    template <typename T>
    void CompileOp(const T *op) {}

    void CompileOp(const ir::IRGetRegisterOp *op);
    void CompileOp(const ir::IRSetRegisterOp *op);
    void CompileOp(const ir::IRGetCPSROp *op);
    void CompileOp(const ir::IRSetCPSROp *op);
    void CompileOp(const ir::IRGetSPSROp *op);
    void CompileOp(const ir::IRSetSPSROp *op);
    void CompileOp(const ir::IRMemReadOp *op);
    void CompileOp(const ir::IRMemWriteOp *op);
    void CompileOp(const ir::IRPreloadOp *op);
    void CompileOp(const ir::IRLogicalShiftLeftOp *op);
    void CompileOp(const ir::IRLogicalShiftRightOp *op);
    void CompileOp(const ir::IRArithmeticShiftRightOp *op);
    void CompileOp(const ir::IRRotateRightOp *op);
    void CompileOp(const ir::IRRotateRightExtendedOp *op);
    void CompileOp(const ir::IRBitwiseAndOp *op);
    void CompileOp(const ir::IRBitwiseOrOp *op);
    void CompileOp(const ir::IRBitwiseXorOp *op);
    void CompileOp(const ir::IRBitClearOp *op);
    void CompileOp(const ir::IRCountLeadingZerosOp *op);
    void CompileOp(const ir::IRAddOp *op);
    void CompileOp(const ir::IRAddCarryOp *op);
    void CompileOp(const ir::IRSubtractOp *op);
    void CompileOp(const ir::IRSubtractCarryOp *op);
    void CompileOp(const ir::IRMoveOp *op);
    void CompileOp(const ir::IRMoveNegatedOp *op);
    void CompileOp(const ir::IRSaturatingAddOp *op);
    void CompileOp(const ir::IRSaturatingSubtractOp *op);
    void CompileOp(const ir::IRMultiplyOp *op);
    void CompileOp(const ir::IRMultiplyLongOp *op);
    void CompileOp(const ir::IRAddLongOp *op);
    void CompileOp(const ir::IRStoreFlagsOp *op);
    void CompileOp(const ir::IRLoadFlagsOp *op);
    void CompileOp(const ir::IRLoadStickyOverflowOp *op);
    void CompileOp(const ir::IRBranchOp *op);
    void CompileOp(const ir::IRBranchExchangeOp *op);
    void CompileOp(const ir::IRLoadCopRegisterOp *op);
    void CompileOp(const ir::IRStoreCopRegisterOp *op);
    void CompileOp(const ir::IRConstantOp *op);
    void CompileOp(const ir::IRCopyVarOp *op);
    void CompileOp(const ir::IRGetBaseVectorAddressOp *op);
};

} // namespace armajitto::x86_64
