#include "armajitto/ir/basic_block.hpp"

#include "armajitto/core/pmr_allocator.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <type_traits>
#include <vector>

namespace armajitto::ir {

// TODO: move this to util
template <typename Fn>
struct ScopeGuard {
    ScopeGuard(Fn &&fn)
        : fn(std::move(fn)) {}

    ~ScopeGuard() {
        if (!cancelled) {
            fn();
        }
    }

    void Cancel() {
        cancelled = true;
    }

private:
    Fn fn;
    bool cancelled = false;
};

void BasicBlock::RenameVariables() {
    uint32_t nextVarID = 0;
    void *ptr = m_alloc.AllocateRaw(m_nextVarID * sizeof(Variable));
    auto *varMap = new (ptr) Variable[m_nextVarID];

    ScopeGuard freePtr{[&] { m_alloc.Free(ptr); }};

    auto mapVar = [&](Variable &var) {
        if (!var.IsPresent()) {
            return;
        }
        auto varIndex = var.Index();
        if (!varMap[varIndex].IsPresent()) {
            varMap[varIndex] = Variable{nextVarID++};
        }
        var = varMap[varIndex];
    };

    auto map = [&]<typename T>(T &arg) {
        if constexpr (std::is_same_v<T, VarOrImmArg>) {
            if (!arg.immediate) {
                mapVar(arg.var.var);
            }
        } else if constexpr (std::is_same_v<T, VariableArg>) {
            mapVar(arg.var);
        } else if constexpr (std::is_same_v<T, Variable>) {
            mapVar(arg);
        }
    };

    IROp *op = m_opsHead;
    while (op != nullptr) {
        switch (op->GetType()) {
        case IROpcodeType::GetRegister: {
            auto opImpl = *Cast<IRGetRegisterOp>(op);
            map(opImpl->dst);
            break;
        }
        case IROpcodeType::SetRegister: {
            auto opImpl = *Cast<IRSetRegisterOp>(op);
            map(opImpl->src);
            break;
        }
        case IROpcodeType::GetCPSR: {
            auto opImpl = *Cast<IRGetCPSROp>(op);
            map(opImpl->dst);
            break;
        }
        case IROpcodeType::SetCPSR: {
            auto opImpl = *Cast<IRSetCPSROp>(op);
            map(opImpl->src);
            break;
        }
        case IROpcodeType::GetSPSR: {
            auto opImpl = *Cast<IRGetSPSROp>(op);
            map(opImpl->dst);
            break;
        }
        case IROpcodeType::SetSPSR: {
            auto opImpl = *Cast<IRSetSPSROp>(op);
            map(opImpl->src);
            break;
        }
        case IROpcodeType::MemRead: {
            auto opImpl = *Cast<IRMemReadOp>(op);
            map(opImpl->dst);
            map(opImpl->address);
            break;
        }
        case IROpcodeType::MemWrite: {
            auto opImpl = *Cast<IRMemWriteOp>(op);
            map(opImpl->src);
            map(opImpl->address);
            break;
        }
        case IROpcodeType::Preload: {
            auto opImpl = *Cast<IRPreloadOp>(op);
            map(opImpl->address);
            break;
        }
        case IROpcodeType::LogicalShiftLeft: {
            auto opImpl = *Cast<IRLogicalShiftLeftOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            map(opImpl->amount);
            break;
        }
        case IROpcodeType::LogicalShiftRight: {
            auto opImpl = *Cast<IRLogicalShiftRightOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            map(opImpl->amount);
            break;
        }
        case IROpcodeType::ArithmeticShiftRight: {
            auto opImpl = *Cast<IRArithmeticShiftRightOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            map(opImpl->amount);
            break;
        }
        case IROpcodeType::RotateRight: {
            auto opImpl = *Cast<IRRotateRightOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            map(opImpl->amount);
            break;
        }
        case IROpcodeType::RotateRightExtended: {
            auto opImpl = *Cast<IRRotateRightExtendedOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            break;
        }
        case IROpcodeType::BitwiseAnd: {
            auto opImpl = *Cast<IRBitwiseAndOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::BitwiseOr: {
            auto opImpl = *Cast<IRBitwiseOrOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::BitwiseXor: {
            auto opImpl = *Cast<IRBitwiseXorOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::BitClear: {
            auto opImpl = *Cast<IRBitClearOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::CountLeadingZeros: {
            auto opImpl = *Cast<IRCountLeadingZerosOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            break;
        }
        case IROpcodeType::Add: {
            auto opImpl = *Cast<IRAddOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::AddCarry: {
            auto opImpl = *Cast<IRAddCarryOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::Subtract: {
            auto opImpl = *Cast<IRSubtractOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::SubtractCarry: {
            auto opImpl = *Cast<IRSubtractCarryOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::Move: {
            auto opImpl = *Cast<IRMoveOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            break;
        }
        case IROpcodeType::MoveNegated: {
            auto opImpl = *Cast<IRMoveNegatedOp>(op);
            map(opImpl->dst);
            map(opImpl->value);
            break;
        }
        case IROpcodeType::SaturatingAdd: {
            auto opImpl = *Cast<IRSaturatingAddOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::SaturatingSubtract: {
            auto opImpl = *Cast<IRSaturatingSubtractOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::Multiply: {
            auto opImpl = *Cast<IRMultiplyOp>(op);
            map(opImpl->dst);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::MultiplyLong: {
            auto opImpl = *Cast<IRMultiplyLongOp>(op);
            map(opImpl->dstLo);
            map(opImpl->dstHi);
            map(opImpl->lhs);
            map(opImpl->rhs);
            break;
        }
        case IROpcodeType::AddLong: {
            auto opImpl = *Cast<IRAddLongOp>(op);
            map(opImpl->dstLo);
            map(opImpl->dstHi);
            map(opImpl->lhsLo);
            map(opImpl->lhsHi);
            map(opImpl->rhsLo);
            map(opImpl->rhsHi);
            break;
        }
        case IROpcodeType::StoreFlags: {
            auto opImpl = *Cast<IRStoreFlagsOp>(op);
            map(opImpl->values);
            break;
        }
        case IROpcodeType::LoadFlags: {
            auto opImpl = *Cast<IRLoadFlagsOp>(op);
            map(opImpl->dstCPSR);
            map(opImpl->srcCPSR);
            break;
        }
        case IROpcodeType::LoadStickyOverflow: {
            auto opImpl = *Cast<IRLoadStickyOverflowOp>(op);
            map(opImpl->dstCPSR);
            map(opImpl->srcCPSR);
            break;
        }
        case IROpcodeType::Branch: {
            auto opImpl = *Cast<IRBranchOp>(op);
            map(opImpl->address);
            break;
        }
        case IROpcodeType::BranchExchange: {
            auto opImpl = *Cast<IRBranchExchangeOp>(op);
            map(opImpl->address);
            break;
        }
        case IROpcodeType::LoadCopRegister: {
            auto opImpl = *Cast<IRLoadCopRegisterOp>(op);
            map(opImpl->dstValue);
            break;
        }
        case IROpcodeType::StoreCopRegister: {
            auto opImpl = *Cast<IRStoreCopRegisterOp>(op);
            map(opImpl->srcValue);
            break;
        }
        case IROpcodeType::Constant: {
            auto opImpl = *Cast<IRConstantOp>(op);
            map(opImpl->dst);
            break;
        }
        case IROpcodeType::CopyVar: {
            auto opImpl = *Cast<IRCopyVarOp>(op);
            map(opImpl->dst);
            map(opImpl->var);
            break;
        }
        case IROpcodeType::GetBaseVectorAddress: {
            auto opImpl = *Cast<IRGetBaseVectorAddressOp>(op);
            map(opImpl->dst);
            break;
        }
        }
        op = op->next;
    }

    m_nextVarID = nextVarID;
}

} // namespace armajitto::ir
