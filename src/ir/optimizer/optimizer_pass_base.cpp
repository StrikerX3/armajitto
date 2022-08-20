#include "optimizer_pass_base.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

bool OptimizerPassBase::Optimize() {
    m_emitter.ClearDirtyFlag();

    PreProcess();

    m_emitter.SetCursorPos(0);
    while (!m_emitter.IsCursorAtEnd()) {
        auto *op = m_emitter.GetCurrentOp();
        if (op == nullptr) {
            assert(false); // shouldn't happen
            continue;
        }

        Process(op);

        if (!m_emitter.IsModifiedSinceLastCursorMove()) {
            m_emitter.MoveCursor(1);
        } else {
            m_emitter.ClearModifiedSinceLastCursorMove();
        }
    }

    PostProcess();

    return m_emitter.IsDirty();
}

void OptimizerPassBase::Process(IROp *op) {
    switch (op->GetType()) {
    case IROpcodeType::GetRegister: Process(*Cast<IRGetRegisterOp>(op)); break;
    case IROpcodeType::SetRegister: Process(*Cast<IRSetRegisterOp>(op)); break;
    case IROpcodeType::GetCPSR: Process(*Cast<IRGetCPSROp>(op)); break;
    case IROpcodeType::SetCPSR: Process(*Cast<IRSetCPSROp>(op)); break;
    case IROpcodeType::GetSPSR: Process(*Cast<IRGetSPSROp>(op)); break;
    case IROpcodeType::SetSPSR: Process(*Cast<IRSetSPSROp>(op)); break;
    case IROpcodeType::MemRead: Process(*Cast<IRMemReadOp>(op)); break;
    case IROpcodeType::MemWrite: Process(*Cast<IRMemWriteOp>(op)); break;
    case IROpcodeType::Preload: Process(*Cast<IRPreloadOp>(op)); break;
    case IROpcodeType::LogicalShiftLeft: Process(*Cast<IRLogicalShiftLeftOp>(op)); break;
    case IROpcodeType::LogicalShiftRight: Process(*Cast<IRLogicalShiftRightOp>(op)); break;
    case IROpcodeType::ArithmeticShiftRight: Process(*Cast<IRArithmeticShiftRightOp>(op)); break;
    case IROpcodeType::RotateRight: Process(*Cast<IRRotateRightOp>(op)); break;
    case IROpcodeType::RotateRightExtend: Process(*Cast<IRRotateRightExtendOp>(op)); break;
    case IROpcodeType::BitwiseAnd: Process(*Cast<IRBitwiseAndOp>(op)); break;
    case IROpcodeType::BitwiseOr: Process(*Cast<IRBitwiseOrOp>(op)); break;
    case IROpcodeType::BitwiseXor: Process(*Cast<IRBitwiseXorOp>(op)); break;
    case IROpcodeType::BitClear: Process(*Cast<IRBitClearOp>(op)); break;
    case IROpcodeType::CountLeadingZeros: Process(*Cast<IRCountLeadingZerosOp>(op)); break;
    case IROpcodeType::Add: Process(*Cast<IRAddOp>(op)); break;
    case IROpcodeType::AddCarry: Process(*Cast<IRAddCarryOp>(op)); break;
    case IROpcodeType::Subtract: Process(*Cast<IRSubtractOp>(op)); break;
    case IROpcodeType::SubtractCarry: Process(*Cast<IRSubtractCarryOp>(op)); break;
    case IROpcodeType::Move: Process(*Cast<IRMoveOp>(op)); break;
    case IROpcodeType::MoveNegated: Process(*Cast<IRMoveNegatedOp>(op)); break;
    case IROpcodeType::SaturatingAdd: Process(*Cast<IRSaturatingAddOp>(op)); break;
    case IROpcodeType::SaturatingSubtract: Process(*Cast<IRSaturatingSubtractOp>(op)); break;
    case IROpcodeType::Multiply: Process(*Cast<IRMultiplyOp>(op)); break;
    case IROpcodeType::MultiplyLong: Process(*Cast<IRMultiplyLongOp>(op)); break;
    case IROpcodeType::AddLong: Process(*Cast<IRAddLongOp>(op)); break;
    case IROpcodeType::StoreFlags: Process(*Cast<IRStoreFlagsOp>(op)); break;
    case IROpcodeType::UpdateFlags: Process(*Cast<IRUpdateFlagsOp>(op)); break;
    case IROpcodeType::UpdateStickyOverflow: Process(*Cast<IRUpdateStickyOverflowOp>(op)); break;
    case IROpcodeType::Branch: Process(*Cast<IRBranchOp>(op)); break;
    case IROpcodeType::BranchExchange: Process(*Cast<IRBranchExchangeOp>(op)); break;
    case IROpcodeType::LoadCopRegister: Process(*Cast<IRLoadCopRegisterOp>(op)); break;
    case IROpcodeType::StoreCopRegister: Process(*Cast<IRStoreCopRegisterOp>(op)); break;
    case IROpcodeType::Constant: Process(*Cast<IRConstantOp>(op)); break;
    case IROpcodeType::CopyVar: Process(*Cast<IRCopyVarOp>(op)); break;
    case IROpcodeType::GetBaseVectorAddress: Process(*Cast<IRGetBaseVectorAddressOp>(op)); break;
    default: break;
    }
}

} // namespace armajitto::ir
