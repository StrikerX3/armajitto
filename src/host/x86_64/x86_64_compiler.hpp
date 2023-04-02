#pragma once

#include "x86_64_host.hpp"

#include "core/memory_map_host_access.hpp"

#include "util/unreachable.hpp"

#include "reg_alloc.hpp"

#ifdef _WIN32
    #define NOMINMAX
#endif
#include <xbyak/xbyak.h>

#include <memory_resource>

namespace armajitto::x86_64 {

class x64Host::Compiler {
public:
    Compiler(Context &context, arm::StateOffsets &stateOffsets, CompiledCode &compiledCode,
             Xbyak::CodeGenerator &codegen, const ir::BasicBlock &block, std::pmr::memory_resource &alloc);

    void PreProcessOp(const ir::IROp *op);
    void PostProcessOp(const ir::IROp *op);

    void CompileGenerationCheck(const LocationRef &baseLoc, const uint32_t instrCount);
    void CompileIRQLineCheck();
    void CompileCondCheck(arm::Condition cond, Xbyak::Label &lblCondFail);
    void CompileTerminal(const ir::BasicBlock &block);
    void CompileDirectLinkToSuccessor(const ir::BasicBlock &block);
    void CompileExit();

    void ReserveTerminalRegisters(const ir::BasicBlock &block);

    void SubtractCycles(uint64_t cycles);

private:
    void CompileDirectLink(LocationRef target, uint64_t blockLocKey);

public:
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

    // -------------------------------------------------------------------------
    // Building blocks

    void SetCFromValue(bool carry);
    void SetCFromFlags(Xbyak::Reg16 tmpReg16);

    void SetVFromValue(bool overflow);
    void SetVFromFlags();

    void SetNZFromValue(uint32_t value);
    void SetNZFromValue(uint64_t value);
    void SetNZFromReg(Xbyak::Reg32 value, Xbyak::Reg32 tmpReg32);
    void SetNZFromFlags(Xbyak::Reg32 tmpReg32);

    void SetNZCVFromValue(uint32_t value, bool carry, bool overflow);
    void SetNZCVFromFlags();

    // Compiles a MOV <reg>, <value> if <value> != 0, or XOR <reg>, <reg> if 0
    void MOVImmediate(Xbyak::Reg32 reg, uint32_t value);

    // Compiles a MOV <dst>, <src> if the registers are different
    void CopyIfDifferent(Xbyak::Reg32 dst, Xbyak::Reg32 src);
    void CopyIfDifferent(Xbyak::Reg64 dst, Xbyak::Reg64 src);

    void AssignImmResultWithNZ(const ir::VariableArg &dst, uint32_t result, bool setFlags);
    void AssignImmResultWithNZCV(const ir::VariableArg &dst, uint32_t result, bool carry, bool overflow, bool setFlags);
    void AssignImmResultWithCarry(const ir::VariableArg &dst, uint32_t result, std::optional<bool> carry,
                                  bool setFlags);
    void AssignImmResultWithOverflow(const ir::VariableArg &dst, uint32_t result, bool overflow, bool setFlags);

    void AssignLongImmResultWithNZ(const ir::VariableArg &dstLo, const ir::VariableArg &dstHi, uint64_t result,
                                   bool setFlags);

    // -------------------------------------------------------------------------
    // Host function calls

    template <typename... FnArgs, typename... Args>
    void CompileInvokeHostFunction(void (*fn)(FnArgs...), Args &&...args) {
        static_assert(args_match_v<arg_list_t<FnArgs...>, arg_list_t<Args...>>, "Incompatible arguments");
        static constexpr Xbyak::Reg noReg{};
        CompileInvokeHostFunctionImpl(noReg, fn, std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... FnArgs, typename... Args>
    void CompileInvokeHostFunction(Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...), Args &&...args) {
        static_assert(args_match_v<arg_list_t<FnArgs...>, arg_list_t<Args...>>, "Incompatible arguments");
        CompileInvokeHostFunctionImpl(dstReg, fn, std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... FnArgs, typename... Args>
    void CompileInvokeHostFunctionImpl(Xbyak::Reg dstReg, ReturnType (*fn)(FnArgs...), Args &&...args);

private:
    Context &m_context;
    CompiledCode &m_compiledCode;
    arm::State &m_armState;
    arm::StateOffsets &m_stateOffsets;
    MemoryMapHostAccess m_memMap;
    Xbyak::CodeGenerator &m_codegen;
    RegisterAllocator m_regAlloc;
    arm::Mode m_mode;
    bool m_thumb;
};

// ---------------------------------------------------------------------------------------------------------------------
// Xbyak helpers

inline Xbyak::Reg8 GetReg8(Xbyak::Reg reg) {
    assert(reg.isREG());
    assert(reg.getIdx() >= 0 && reg.getIdx() <= 15);

    switch (reg.getIdx()) {
    case 0: return al;
    case 1: return cl;
    case 2: return dl;
    case 3: return bl;
    case 4: return spl;
    case 5: return bpl;
    case 6: return sil;
    case 7: return dil;
    case 8: return r8b;
    case 9: return r9b;
    case 10: return r10b;
    case 11: return r11b;
    case 12: return r12b;
    case 13: return r13b;
    case 14: return r14b;
    case 15: return r15b;
    default: util::unreachable();
    }
}

} // namespace armajitto::x86_64
