#pragma once

#include "ir/ir_ops.hpp"

#include <type_traits>

namespace armajitto::ir {

template <typename Visitor, typename ReturnType = std::invoke_result_t<Visitor, IROp *>>
ReturnType VisitIROp(IROp *op, Visitor &&visitor) {
    if (op != nullptr) {
        switch (op->type) {
        case IROpcodeType::GetRegister: return visitor(Cast<IRGetRegisterOp>(op));
        case IROpcodeType::SetRegister: return visitor(Cast<IRSetRegisterOp>(op));
        case IROpcodeType::GetCPSR: return visitor(Cast<IRGetCPSROp>(op));
        case IROpcodeType::SetCPSR: return visitor(Cast<IRSetCPSROp>(op));
        case IROpcodeType::GetSPSR: return visitor(Cast<IRGetSPSROp>(op));
        case IROpcodeType::SetSPSR: return visitor(Cast<IRSetSPSROp>(op));
        case IROpcodeType::MemRead: return visitor(Cast<IRMemReadOp>(op));
        case IROpcodeType::MemWrite: return visitor(Cast<IRMemWriteOp>(op));
        case IROpcodeType::Preload: return visitor(Cast<IRPreloadOp>(op));
        case IROpcodeType::LogicalShiftLeft: return visitor(Cast<IRLogicalShiftLeftOp>(op));
        case IROpcodeType::LogicalShiftRight: return visitor(Cast<IRLogicalShiftRightOp>(op));
        case IROpcodeType::ArithmeticShiftRight: return visitor(Cast<IRArithmeticShiftRightOp>(op));
        case IROpcodeType::RotateRight: return visitor(Cast<IRRotateRightOp>(op));
        case IROpcodeType::RotateRightExtended: return visitor(Cast<IRRotateRightExtendedOp>(op));
        case IROpcodeType::BitwiseAnd: return visitor(Cast<IRBitwiseAndOp>(op));
        case IROpcodeType::BitwiseOr: return visitor(Cast<IRBitwiseOrOp>(op));
        case IROpcodeType::BitwiseXor: return visitor(Cast<IRBitwiseXorOp>(op));
        case IROpcodeType::BitClear: return visitor(Cast<IRBitClearOp>(op));
        case IROpcodeType::CountLeadingZeros: return visitor(Cast<IRCountLeadingZerosOp>(op));
        case IROpcodeType::Add: return visitor(Cast<IRAddOp>(op));
        case IROpcodeType::AddCarry: return visitor(Cast<IRAddCarryOp>(op));
        case IROpcodeType::Subtract: return visitor(Cast<IRSubtractOp>(op));
        case IROpcodeType::SubtractCarry: return visitor(Cast<IRSubtractCarryOp>(op));
        case IROpcodeType::Move: return visitor(Cast<IRMoveOp>(op));
        case IROpcodeType::MoveNegated: return visitor(Cast<IRMoveNegatedOp>(op));
        case IROpcodeType::SaturatingAdd: return visitor(Cast<IRSaturatingAddOp>(op));
        case IROpcodeType::SaturatingSubtract: return visitor(Cast<IRSaturatingSubtractOp>(op));
        case IROpcodeType::Multiply: return visitor(Cast<IRMultiplyOp>(op));
        case IROpcodeType::MultiplyLong: return visitor(Cast<IRMultiplyLongOp>(op));
        case IROpcodeType::AddLong: return visitor(Cast<IRAddLongOp>(op));
        case IROpcodeType::StoreFlags: return visitor(Cast<IRStoreFlagsOp>(op));
        case IROpcodeType::LoadFlags: return visitor(Cast<IRLoadFlagsOp>(op));
        case IROpcodeType::LoadStickyOverflow: return visitor(Cast<IRLoadStickyOverflowOp>(op));
        case IROpcodeType::Branch: return visitor(Cast<IRBranchOp>(op));
        case IROpcodeType::BranchExchange: return visitor(Cast<IRBranchExchangeOp>(op));
        case IROpcodeType::LoadCopRegister: return visitor(Cast<IRLoadCopRegisterOp>(op));
        case IROpcodeType::StoreCopRegister: return visitor(Cast<IRStoreCopRegisterOp>(op));
        case IROpcodeType::Constant: return visitor(Cast<IRConstantOp>(op));
        case IROpcodeType::CopyVar: return visitor(Cast<IRCopyVarOp>(op));
        case IROpcodeType::GetBaseVectorAddress: return visitor(Cast<IRGetBaseVectorAddressOp>(op));
        }
    }

    if constexpr (!std::is_void_v<ReturnType>) {
        static_assert(std::is_default_constructible_v<ReturnType>,
                      "The visitor must return void or a default constructible object");
        return ReturnType{};
    }
}

template <typename Visitor, typename ReturnType = std::invoke_result_t<Visitor, const IROp *>>
ReturnType VisitIROp(const IROp *op, Visitor &&visitor) {
    return VisitIROp(const_cast<IROp *>(op), visitor);
}

// ---------------------------------------------------------------------------------------------------------------------

namespace detail {
    template <typename Visitor>
    void VisitVar(IROp *op, const ir::VariableArg &arg, bool read, Visitor &&visitor) {
        if (arg.var.IsPresent()) {
            visitor(op, arg.var, read);
        }
    }

    template <typename Visitor>
    void VisitVar(IROp *op, const ir::VarOrImmArg &arg, bool read, Visitor &&visitor) {
        if (!arg.immediate) {
            VisitVar(op, arg.var, read, visitor);
        }
    }

    template <bool writesFirst, typename T, typename Visitor>
    void VisitIROpVars(T *op, Visitor &&visitor) {}

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRGetRegisterOp *op, Visitor &&visitor) {
        VisitVar(op, op->dst, false, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSetRegisterOp *op, Visitor &&visitor) {
        VisitVar(op, op->src, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRGetCPSROp *op, Visitor &&visitor) {
        VisitVar(op, op->dst, false, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSetCPSROp *op, Visitor &&visitor) {
        VisitVar(op, op->src, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRGetSPSROp *op, Visitor &&visitor) {
        VisitVar(op, op->dst, false, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSetSPSROp *op, Visitor &&visitor) {
        VisitVar(op, op->src, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMemReadOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->address, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMemWriteOp *op, Visitor &&visitor) {
        VisitVar(op, op->src, false, visitor);
        VisitVar(op, op->address, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRPreloadOp *op, Visitor &&visitor) {
        VisitVar(op, op->address, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRLogicalShiftLeftOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        VisitVar(op, op->amount, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRLogicalShiftRightOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        VisitVar(op, op->amount, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRArithmeticShiftRightOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        VisitVar(op, op->amount, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRRotateRightOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        VisitVar(op, op->amount, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRRotateRightExtendedOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBitwiseAndOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBitwiseOrOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBitwiseXorOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBitClearOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRCountLeadingZerosOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRAddOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRAddCarryOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSubtractOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSubtractCarryOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMoveOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMoveNegatedOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->value, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSaturatingAddOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRSaturatingSubtractOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMultiplyOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRMultiplyLongOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dstLo, false, visitor);
            VisitVar(op, op->dstHi, false, visitor);
        }
        VisitVar(op, op->lhs, true, visitor);
        VisitVar(op, op->rhs, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dstLo, false, visitor);
            VisitVar(op, op->dstHi, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRAddLongOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dstLo, false, visitor);
            VisitVar(op, op->dstHi, false, visitor);
        }
        VisitVar(op, op->lhsLo, true, visitor);
        VisitVar(op, op->lhsHi, true, visitor);
        VisitVar(op, op->rhsLo, true, visitor);
        VisitVar(op, op->rhsHi, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dstLo, false, visitor);
            VisitVar(op, op->dstHi, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRStoreFlagsOp *op, Visitor &&visitor) {
        VisitVar(op, op->values, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRLoadFlagsOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dstCPSR, false, visitor);
        }
        VisitVar(op, op->srcCPSR, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dstCPSR, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRLoadStickyOverflowOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dstCPSR, false, visitor);
        }
        VisitVar(op, op->srcCPSR, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dstCPSR, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBranchOp *op, Visitor &&visitor) {
        VisitVar(op, op->address, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRBranchExchangeOp *op, Visitor &&visitor) {
        VisitVar(op, op->address, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRLoadCopRegisterOp *op, Visitor &&visitor) {
        VisitVar(op, op->dstValue, false, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRStoreCopRegisterOp *op, Visitor &&visitor) {
        VisitVar(op, op->srcValue, true, visitor);
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRCopyVarOp *op, Visitor &&visitor) {
        if constexpr (writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
        VisitVar(op, op->var, true, visitor);
        if constexpr (!writesFirst) {
            VisitVar(op, op->dst, false, visitor);
        }
    }

    template <bool writesFirst, typename Visitor>
    void VisitIROpVars(ir::IRGetBaseVectorAddressOp *op, Visitor &&visitor) {
        VisitVar(op, op->dst, false, visitor);
    }

} // namespace detail

// Visitor signature: void(<IROp subclass> *op, Variable var, bool read)
template <bool writesFirst = true, typename Visitor>
void VisitIROpVars(IROp *op, Visitor &&visitor) {
    VisitIROp(op, [visitor](auto *op) { detail::VisitIROpVars<writesFirst>(op, visitor); });
}

template <bool writesFirst = true, typename Visitor>
void VisitIROpVars(const IROp *op, Visitor &&visitor) {
    VisitIROpVars<writesFirst>(const_cast<IROp *>(op), visitor);
}

} // namespace armajitto::ir
