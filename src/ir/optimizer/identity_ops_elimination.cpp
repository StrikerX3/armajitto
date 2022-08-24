#include "identity_ops_elimination.hpp"

namespace armajitto::ir {

IdentityOpsEliminationOptimizerPass::IdentityOpsEliminationOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter)
    , m_varSubst(emitter.VariableCount()) {}

void IdentityOpsEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRRotateRightOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRBitClearOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRAddOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRAddCarryOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRSubtractOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRMultiplyOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {}

void IdentityOpsEliminationOptimizerPass::Process(IRAddLongOp *op) {}

} // namespace armajitto::ir
