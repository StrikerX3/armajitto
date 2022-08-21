#pragma once

#include "armajitto/ir/ir_ops.hpp"

namespace armajitto::ir {

template <typename Visitor>
void VisitIROp(IROp *op, Visitor &&visitor) {
    switch (op->GetType()) {
    case IROpcodeType::GetRegister: visitor(*Cast<IRGetRegisterOp>(op)); break;
    case IROpcodeType::SetRegister: visitor(*Cast<IRSetRegisterOp>(op)); break;
    case IROpcodeType::GetCPSR: visitor(*Cast<IRGetCPSROp>(op)); break;
    case IROpcodeType::SetCPSR: visitor(*Cast<IRSetCPSROp>(op)); break;
    case IROpcodeType::GetSPSR: visitor(*Cast<IRGetSPSROp>(op)); break;
    case IROpcodeType::SetSPSR: visitor(*Cast<IRSetSPSROp>(op)); break;
    case IROpcodeType::MemRead: visitor(*Cast<IRMemReadOp>(op)); break;
    case IROpcodeType::MemWrite: visitor(*Cast<IRMemWriteOp>(op)); break;
    case IROpcodeType::Preload: visitor(*Cast<IRPreloadOp>(op)); break;
    case IROpcodeType::LogicalShiftLeft: visitor(*Cast<IRLogicalShiftLeftOp>(op)); break;
    case IROpcodeType::LogicalShiftRight: visitor(*Cast<IRLogicalShiftRightOp>(op)); break;
    case IROpcodeType::ArithmeticShiftRight: visitor(*Cast<IRArithmeticShiftRightOp>(op)); break;
    case IROpcodeType::RotateRight: visitor(*Cast<IRRotateRightOp>(op)); break;
    case IROpcodeType::RotateRightExtend: visitor(*Cast<IRRotateRightExtendOp>(op)); break;
    case IROpcodeType::BitwiseAnd: visitor(*Cast<IRBitwiseAndOp>(op)); break;
    case IROpcodeType::BitwiseOr: visitor(*Cast<IRBitwiseOrOp>(op)); break;
    case IROpcodeType::BitwiseXor: visitor(*Cast<IRBitwiseXorOp>(op)); break;
    case IROpcodeType::BitClear: visitor(*Cast<IRBitClearOp>(op)); break;
    case IROpcodeType::CountLeadingZeros: visitor(*Cast<IRCountLeadingZerosOp>(op)); break;
    case IROpcodeType::Add: visitor(*Cast<IRAddOp>(op)); break;
    case IROpcodeType::AddCarry: visitor(*Cast<IRAddCarryOp>(op)); break;
    case IROpcodeType::Subtract: visitor(*Cast<IRSubtractOp>(op)); break;
    case IROpcodeType::SubtractCarry: visitor(*Cast<IRSubtractCarryOp>(op)); break;
    case IROpcodeType::Move: visitor(*Cast<IRMoveOp>(op)); break;
    case IROpcodeType::MoveNegated: visitor(*Cast<IRMoveNegatedOp>(op)); break;
    case IROpcodeType::SaturatingAdd: visitor(*Cast<IRSaturatingAddOp>(op)); break;
    case IROpcodeType::SaturatingSubtract: visitor(*Cast<IRSaturatingSubtractOp>(op)); break;
    case IROpcodeType::Multiply: visitor(*Cast<IRMultiplyOp>(op)); break;
    case IROpcodeType::MultiplyLong: visitor(*Cast<IRMultiplyLongOp>(op)); break;
    case IROpcodeType::AddLong: visitor(*Cast<IRAddLongOp>(op)); break;
    case IROpcodeType::StoreFlags: visitor(*Cast<IRStoreFlagsOp>(op)); break;
    case IROpcodeType::UpdateFlags: visitor(*Cast<IRUpdateFlagsOp>(op)); break;
    case IROpcodeType::UpdateStickyOverflow: visitor(*Cast<IRUpdateStickyOverflowOp>(op)); break;
    case IROpcodeType::Branch: visitor(*Cast<IRBranchOp>(op)); break;
    case IROpcodeType::BranchExchange: visitor(*Cast<IRBranchExchangeOp>(op)); break;
    case IROpcodeType::LoadCopRegister: visitor(*Cast<IRLoadCopRegisterOp>(op)); break;
    case IROpcodeType::StoreCopRegister: visitor(*Cast<IRStoreCopRegisterOp>(op)); break;
    case IROpcodeType::Constant: visitor(*Cast<IRConstantOp>(op)); break;
    case IROpcodeType::CopyVar: visitor(*Cast<IRCopyVarOp>(op)); break;
    case IROpcodeType::GetBaseVectorAddress: visitor(*Cast<IRGetBaseVectorAddressOp>(op)); break;
    }
}

} // namespace armajitto::ir
