#pragma once

#include "armajitto/ir/ir_ops.hpp"

#include <type_traits>

namespace armajitto::ir {

template <typename Visitor, typename ReturnType = std::invoke_result_t<Visitor, IROp *>>
ReturnType VisitIROp(IROp *op, Visitor &&visitor) {
    if (op != nullptr) {
        switch (op->GetType()) {
        case IROpcodeType::GetRegister: return visitor(*Cast<IRGetRegisterOp>(op));
        case IROpcodeType::SetRegister: return visitor(*Cast<IRSetRegisterOp>(op));
        case IROpcodeType::GetCPSR: return visitor(*Cast<IRGetCPSROp>(op));
        case IROpcodeType::SetCPSR: return visitor(*Cast<IRSetCPSROp>(op));
        case IROpcodeType::GetSPSR: return visitor(*Cast<IRGetSPSROp>(op));
        case IROpcodeType::SetSPSR: return visitor(*Cast<IRSetSPSROp>(op));
        case IROpcodeType::MemRead: return visitor(*Cast<IRMemReadOp>(op));
        case IROpcodeType::MemWrite: return visitor(*Cast<IRMemWriteOp>(op));
        case IROpcodeType::Preload: return visitor(*Cast<IRPreloadOp>(op));
        case IROpcodeType::LogicalShiftLeft: return visitor(*Cast<IRLogicalShiftLeftOp>(op));
        case IROpcodeType::LogicalShiftRight: return visitor(*Cast<IRLogicalShiftRightOp>(op));
        case IROpcodeType::ArithmeticShiftRight: return visitor(*Cast<IRArithmeticShiftRightOp>(op));
        case IROpcodeType::RotateRight: return visitor(*Cast<IRRotateRightOp>(op));
        case IROpcodeType::RotateRightExtend: return visitor(*Cast<IRRotateRightExtendOp>(op));
        case IROpcodeType::BitwiseAnd: return visitor(*Cast<IRBitwiseAndOp>(op));
        case IROpcodeType::BitwiseOr: return visitor(*Cast<IRBitwiseOrOp>(op));
        case IROpcodeType::BitwiseXor: return visitor(*Cast<IRBitwiseXorOp>(op));
        case IROpcodeType::BitClear: return visitor(*Cast<IRBitClearOp>(op));
        case IROpcodeType::CountLeadingZeros: return visitor(*Cast<IRCountLeadingZerosOp>(op));
        case IROpcodeType::Add: return visitor(*Cast<IRAddOp>(op));
        case IROpcodeType::AddCarry: return visitor(*Cast<IRAddCarryOp>(op));
        case IROpcodeType::Subtract: return visitor(*Cast<IRSubtractOp>(op));
        case IROpcodeType::SubtractCarry: return visitor(*Cast<IRSubtractCarryOp>(op));
        case IROpcodeType::Move: return visitor(*Cast<IRMoveOp>(op));
        case IROpcodeType::MoveNegated: return visitor(*Cast<IRMoveNegatedOp>(op));
        case IROpcodeType::SaturatingAdd: return visitor(*Cast<IRSaturatingAddOp>(op));
        case IROpcodeType::SaturatingSubtract: return visitor(*Cast<IRSaturatingSubtractOp>(op));
        case IROpcodeType::Multiply: return visitor(*Cast<IRMultiplyOp>(op));
        case IROpcodeType::MultiplyLong: return visitor(*Cast<IRMultiplyLongOp>(op));
        case IROpcodeType::AddLong: return visitor(*Cast<IRAddLongOp>(op));
        case IROpcodeType::StoreFlags: return visitor(*Cast<IRStoreFlagsOp>(op));
        case IROpcodeType::LoadFlags: return visitor(*Cast<IRLoadFlagsOp>(op));
        case IROpcodeType::LoadStickyOverflow: return visitor(*Cast<IRLoadStickyOverflowOp>(op));
        case IROpcodeType::Branch: return visitor(*Cast<IRBranchOp>(op));
        case IROpcodeType::BranchExchange: return visitor(*Cast<IRBranchExchangeOp>(op));
        case IROpcodeType::LoadCopRegister: return visitor(*Cast<IRLoadCopRegisterOp>(op));
        case IROpcodeType::StoreCopRegister: return visitor(*Cast<IRStoreCopRegisterOp>(op));
        case IROpcodeType::Constant: return visitor(*Cast<IRConstantOp>(op));
        case IROpcodeType::CopyVar: return visitor(*Cast<IRCopyVarOp>(op));
        case IROpcodeType::GetBaseVectorAddress: return visitor(*Cast<IRGetBaseVectorAddressOp>(op));
        }
    }

    if constexpr (!std::is_void_v<ReturnType>) {
        static_assert(std::is_default_constructible_v<ReturnType>,
                      "The visitor must return void or a default constructible object");
        return ReturnType{};
    }
}

} // namespace armajitto::ir
