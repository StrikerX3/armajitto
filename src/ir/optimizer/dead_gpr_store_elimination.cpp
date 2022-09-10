#include "dead_gpr_store_elimination.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadGPRStoreEliminationOptimizerPass::DeadGPRStoreEliminationOptimizerPass(Emitter &emitter)
    : DeadStoreEliminationOptimizerPassBase(emitter) {
    Reset();
}

void DeadGPRStoreEliminationOptimizerPass::Reset() {
    m_gprWrites.fill(nullptr);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordGPRRead(op->src);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordGPRWrite(op->dst, op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordGPRWrite(arm::GPR::PC, op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordGPRWrite(arm::GPR::PC, op);
}

// ---------------------------------------------------------------------------------------------------------------------
// GPR read and write tracking

void DeadGPRStoreEliminationOptimizerPass::RecordGPRRead(GPRArg gpr) {
    m_gprWrites[gpr.Index()] = nullptr; // Leave instruction alone
}

void DeadGPRStoreEliminationOptimizerPass::RecordGPRWrite(GPRArg gpr, IROp *op) {
    auto gprIndex = gpr.Index();
    IROp *writeOp = m_gprWrites[gprIndex];
    if (writeOp != nullptr) {
        // GPR is overwritten
        // Erase previous instruction, which is always going to be an IRSetRegisterOp
        m_emitter.Erase(writeOp);
    }
    m_gprWrites[gprIndex] = op;
}

} // namespace armajitto::ir
