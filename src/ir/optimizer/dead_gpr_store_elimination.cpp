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
    m_psrWrites.fill(nullptr);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordGPRRead(op->src);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordGPRWrite(op->dst, op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead();
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordCPSRWrite(op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordSPSRWrite(op->mode, op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordCPSRRead();
    RecordGPRWrite(arm::GPR::PC, op);
}

void DeadGPRStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordCPSRRead();
    RecordGPRWrite(arm::GPR::PC, op);
    RecordCPSRWrite(op);
}

// ---------------------------------------------------------------------------------------------------------------------
// GPR and PSR read and write tracking

static inline size_t SPSRIndex(arm::Mode mode) {
    return arm::NormalizedIndex(mode) + 1;
}

void DeadGPRStoreEliminationOptimizerPass::RecordGPRRead(GPRArg gpr) {
    m_gprWrites[gpr.Index()] = nullptr; // Leave instruction alone
}

void DeadGPRStoreEliminationOptimizerPass::RecordCPSRRead() {
    RecordPSRRead(0);
}

void DeadGPRStoreEliminationOptimizerPass::RecordSPSRRead(arm::Mode mode) {
    RecordPSRRead(SPSRIndex(mode));
}

void DeadGPRStoreEliminationOptimizerPass::RecordPSRRead(size_t index) {
    m_psrWrites[index] = nullptr; // Leave instruction alone
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

void DeadGPRStoreEliminationOptimizerPass::RecordCPSRWrite(IROp *op) {
    RecordPSRWrite(0, op);
}

void DeadGPRStoreEliminationOptimizerPass::RecordSPSRWrite(arm::Mode mode, IROp *op) {
    RecordPSRWrite(SPSRIndex(mode), op);
}

void DeadGPRStoreEliminationOptimizerPass::RecordPSRWrite(size_t index, IROp *op) {
    IROp *writeOp = m_psrWrites[index];
    if (writeOp != nullptr) {
        // PSR is overwritten
        // Erase previous instruction if it is a PSR write
        if (writeOp->type == IROpcodeType::SetCPSR || writeOp->type == IROpcodeType::SetSPSR) {
            m_emitter.Erase(writeOp);
        }
    }
    m_psrWrites[index] = op;
}

} // namespace armajitto::ir
