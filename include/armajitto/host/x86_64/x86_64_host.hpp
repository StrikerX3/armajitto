#pragma once

#include "armajitto/host/host.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ir_ops.hpp"
#include "armajitto/util/pointer_cast.hpp"
#include "armajitto/util/type_traits.hpp"

#include "x86_64_type_traits.hpp"

#ifdef _WIN32
    #define NOMINMAX
#endif
#include <xbyak/xbyak.h>

// TODO: I'll probably regret this...
#include <unordered_map>

namespace armajitto::x86_64 {

class x64Host final : public Host {
public:
    x64Host(Context &context);

    HostCode Compile(ir::BasicBlock &block) final;

    void Call(LocationRef loc) final {
        auto code = GetCodeForLocation(m_blockCache, loc.ToUint64());
        Call(code);
    }

    void Call(HostCode code) final {
        if (code != 0) {
            m_prolog(code);
        }
    }

private:
    using PrologFn = void (*)(uintptr_t blockFn);
    PrologFn m_prolog;
    HostCode m_epilog;
    uint64_t m_stackAlignmentOffset;

    struct XbyakAllocator final : Xbyak::Allocator {
        using Xbyak::Allocator::Allocator;

        virtual uint8_t *alloc(size_t size) {
            return reinterpret_cast<uint8_t *>(allocator.AllocateRaw(size, 4096u));
        }

        virtual void free(uint8_t *p) {
            allocator.Free(p);
        }

        memory::Allocator allocator;
    };

    XbyakAllocator m_alloc;
    Xbyak::CodeGenerator m_codegen;

    struct CachedBlock {
        HostCode code;
    };

    // TODO: redesign cache to a simpler design that can be easily traversed in handwritten assembly
    std::unordered_map<uint64_t, CachedBlock> m_blockCache;

    static HostCode GetCodeForLocation(std::unordered_map<uint64_t, CachedBlock> &blockCache, uint64_t lochash) {
        auto it = blockCache.find(lochash);
        if (it != blockCache.end()) {
            return it->second.code;
        } else {
            return CastUintPtr(nullptr);
        }
    }

    struct Compiler;

    void CompileProlog();
    void CompileEpilog();

    void CompileCondCheck(arm::Condition cond, Xbyak::Label &lblCondFail);

    void CompileBlockCacheLookup(Compiler &compiler);

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

    // -------------------------------------------------------------------------
    // Building blocks

    void SetCFromValue(bool carry);
    void SetCFromFlags(Compiler &compiler);

    void SetVFromValue(bool overflow);
    void SetVFromFlags();

    void SetNZFromValue(uint32_t value);
    void SetNZFromValue(uint64_t value);
    void SetNZFromReg(Compiler &compiler, Xbyak::Reg32 value);
    void SetNZFromFlags(Compiler &compiler);

    void SetNZCVFromValue(uint32_t value, bool carry, bool overflow);
    void SetNZCVFromFlags();

    // Compiles a MOV <reg>, <value> if <value> != 0, or XOR <reg>, <reg> if 0
    void MOVImmediate(Xbyak::Reg32 reg, uint32_t value);

    // Compiles a MOV <dst>, <src> if the registers are different
    void CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src);
    void CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src);

    void AssignImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool setFlags);
    void AssignImmResultWithNZCV(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool carry,
                                 bool overflow, bool setFlags);
    void AssignImmResultWithCarry(Compiler &compiler, const ir::VariableArg &dst, uint32_t result,
                                  std::optional<bool> carry, bool setFlags);
    void AssignImmResultWithOverflow(Compiler &compiler, const ir::VariableArg &dst, uint32_t result, bool overflow,
                                     bool setFlags);

    void AssignLongImmResultWithNZ(Compiler &compiler, const ir::VariableArg &dstLo, const ir::VariableArg &dstHi,
                                   uint64_t result, bool setFlags);

    // -------------------------------------------------------------------------
    // Host function calls

    template <typename... FnArgs, typename... Args>
    void CompileInvokeHostFunction(Compiler &compiler, void (*fn)(FnArgs...), Args &&...args) {
        static_assert(args_match_v<arg_list_t<FnArgs...>, arg_list_t<Args...>>, "Incompatible arguments");
        static constexpr Xbyak::Reg noReg{};
        CompileInvokeHostFunctionImpl(compiler, noReg, fn, std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... FnArgs, typename... Args>
    void CompileInvokeHostFunction(Compiler &compiler, Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...), Args &&...args) {
        static_assert(args_match_v<arg_list_t<FnArgs...>, arg_list_t<Args...>>, "Incompatible arguments");
        CompileInvokeHostFunctionImpl(compiler, dstReg, fn, std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... FnArgs, typename... Args>
    void CompileInvokeHostFunctionImpl(Compiler &compiler, Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...),
                                       Args &&...args);
};

} // namespace armajitto::x86_64
