#pragma once

#include "host/host.hpp"

#include "core/memory_map_host_access.hpp"

#include "ir/defs/arguments.hpp"
#include "ir/ir_ops.hpp"

#include "util/pointer_cast.hpp"
#include "util/type_traits.hpp"

#include <map>
#include <memory_resource>
#include <variant>
#include <vector>

namespace armajitto::interp {

class InterpreterHost final : public Host {
public:
    InterpreterHost(Context &context, Options::Compiler &options, std::pmr::memory_resource &alloc);
    ~InterpreterHost();

    HostCode Compile(ir::BasicBlock &block) final;

    HostCode GetCodeForLocation(LocationRef loc) final {
        auto it = m_blockCache.find(loc.ToUint64());
        if (it != m_blockCache.end()) {
            return HostCode(it->first);
        } else {
            return nullptr;
        }
    }

    int64_t Call(LocationRef loc, uint64_t cycles) final {
        auto code = GetCodeForLocation(loc);
        return Call(code, cycles);
    }

    int64_t Call(HostCode code, uint64_t cycles) final {
        for (auto &cacheInvalidation : m_cacheInvalidations) {
            if (cacheInvalidation.start == 0 && cacheInvalidation.end == 0xFFFFFFFF) {
                m_blockCache.clear();
                break;
            }

            for (uint64_t cpsr = 0; cpsr < 63; cpsr++) {
                const uint64_t upper = cpsr << 32ull;
                const uint64_t keyStart = cacheInvalidation.start | upper;
                const uint64_t keyEnd = cacheInvalidation.end | upper;
                auto it = m_blockCache.lower_bound(keyStart);
                while (it != m_blockCache.end()) {
                    if (it->first >= keyStart && it->first <= keyEnd) {
                        it = m_blockCache.erase(it);
                    } else {
                        break;
                    }
                }
            }
        }
        m_cacheInvalidations.clear();

        if (m_armState.IRQLine()) {
            m_armState.ExecutionState() = arm::ExecState::Running;
            if (!m_armState.CPSR().i) {
                m_armState.EnterException(arm::Exception::NormalInterrupt);
                return cycles - 1;
            }
        }
        if (m_armState.ExecutionState() != arm::ExecState::Running) {
            return 0;
        }

        m_flags = static_cast<arm::Flags>(m_armState.CPSR().u32 & 0xF0000000);
        m_flagQ = m_armState.CPSR().q;

        auto it = m_blockCache.find(uint64_t(code));
        if (it != m_blockCache.end()) {
            cycles -= Execute(it->second);
        }

        return cycles;
    }

    void Clear() final;

    void Invalidate(LocationRef loc) final;
    void InvalidateCodeCache() final;
    void InvalidateCodeCacheRange(uint32_t start, uint32_t end) final;

    void ReportMemoryWrite(uint32_t start, uint32_t end) final;

private:
    std::pmr::memory_resource &m_alloc;
    arm::State &m_armState;
    MemoryMapHostAccess m_memMap;

    struct CacheInvalidation {
        enum Type { AddrRange, Location };

        Type type;
        uint32_t start;
        uint32_t end;
        LocationRef location;

        CacheInvalidation(uint32_t start, uint32_t end)
            : type(Type::AddrRange)
            , start(start)
            , end(end) {}

        CacheInvalidation(LocationRef loc)
            : type(Type::Location)
            , start(0)
            , end(0)
            , location(loc) {}

        CacheInvalidation(const CacheInvalidation &other) = default;

        CacheInvalidation &operator=(const CacheInvalidation &other) = default;
    };

    std::pmr::vector<CacheInvalidation> m_cacheInvalidations;

    std::pmr::vector<uint32_t> m_vars;
    arm::Flags m_flags = arm::Flags::None;
    bool m_flagQ = false;

    void SetVar(ir::Variable var, uint32_t value);
    uint32_t GetVar(ir::Variable var);

    uint32_t Get(ir::VarOrImmArg arg);
    uint32_t Get(ir::VariableArg arg);
    uint32_t Get(ir::ImmediateArg arg);

    using Op = std::variant<
        // Register access
        ir::IRGetRegisterOp, ir::IRSetRegisterOp, ir::IRGetCPSROp, ir::IRSetCPSROp, ir::IRGetSPSROp, ir::IRSetSPSROp,

        // Memory access
        ir::IRMemReadOp, ir::IRMemWriteOp, ir::IRPreloadOp,

        // ALU operations
        ir::IRLogicalShiftLeftOp, ir::IRLogicalShiftRightOp, ir::IRArithmeticShiftRightOp, ir::IRRotateRightOp,
        ir::IRRotateRightExtendedOp,

        ir::IRBitwiseAndOp, ir::IRBitwiseOrOp, ir::IRBitwiseXorOp, ir::IRBitClearOp, ir::IRCountLeadingZerosOp,

        ir::IRAddOp, ir::IRAddCarryOp, ir::IRSubtractOp, ir::IRSubtractCarryOp,

        ir::IRMoveOp, ir::IRMoveNegatedOp,

        ir::IRSaturatingAddOp, ir::IRSaturatingSubtractOp,

        ir::IRMultiplyOp, ir::IRMultiplyLongOp, ir::IRAddLongOp,

        // Flag manipulation
        ir::IRStoreFlagsOp, ir::IRLoadFlagsOp, ir::IRLoadStickyOverflowOp,

        // Branching
        ir::IRBranchOp, ir::IRBranchExchangeOp,

        // Coprocessor operations
        ir::IRLoadCopRegisterOp, ir::IRStoreCopRegisterOp,

        // Miscellaneous operations
        ir::IRConstantOp, ir::IRCopyVarOp, ir::IRGetBaseVectorAddressOp>;

    using FnIROpHandler = void (InterpreterHost::*)(const Op &op, LocationRef loc);

    struct InterpInstr {
        FnIROpHandler fn;
        Op op;
    };

    struct CompiledBlock {
        arm::Condition cond;
        int64_t passCycles;
        int64_t failCycles;
        LocationRef loc;
        uint32_t instrCount;

        std::pmr::vector<InterpInstr> instrs;
    };

    std::pmr::map<uint64_t, CompiledBlock> m_blockCache;

    int64_t Execute(const CompiledBlock &block);

    // -------------------------------------------------------------------------------------------
    // IR opcode compilers

    // Catch-all method for unimplemented ops, required by the visitor
    template <typename T>
    InterpInstr CompileOp(const T *op) {}

    InterpInstr CompileOp(const ir::IRGetRegisterOp *op);
    InterpInstr CompileOp(const ir::IRSetRegisterOp *op);
    InterpInstr CompileOp(const ir::IRGetCPSROp *op);
    InterpInstr CompileOp(const ir::IRSetCPSROp *op);
    InterpInstr CompileOp(const ir::IRGetSPSROp *op);
    InterpInstr CompileOp(const ir::IRSetSPSROp *op);
    InterpInstr CompileOp(const ir::IRMemReadOp *op);
    InterpInstr CompileOp(const ir::IRMemWriteOp *op);
    InterpInstr CompileOp(const ir::IRPreloadOp *op);
    InterpInstr CompileOp(const ir::IRLogicalShiftLeftOp *op);
    InterpInstr CompileOp(const ir::IRLogicalShiftRightOp *op);
    InterpInstr CompileOp(const ir::IRArithmeticShiftRightOp *op);
    InterpInstr CompileOp(const ir::IRRotateRightOp *op);
    InterpInstr CompileOp(const ir::IRRotateRightExtendedOp *op);
    InterpInstr CompileOp(const ir::IRBitwiseAndOp *op);
    InterpInstr CompileOp(const ir::IRBitwiseOrOp *op);
    InterpInstr CompileOp(const ir::IRBitwiseXorOp *op);
    InterpInstr CompileOp(const ir::IRBitClearOp *op);
    InterpInstr CompileOp(const ir::IRCountLeadingZerosOp *op);
    InterpInstr CompileOp(const ir::IRAddOp *op);
    InterpInstr CompileOp(const ir::IRAddCarryOp *op);
    InterpInstr CompileOp(const ir::IRSubtractOp *op);
    InterpInstr CompileOp(const ir::IRSubtractCarryOp *op);
    InterpInstr CompileOp(const ir::IRMoveOp *op);
    InterpInstr CompileOp(const ir::IRMoveNegatedOp *op);
    InterpInstr CompileOp(const ir::IRSaturatingAddOp *op);
    InterpInstr CompileOp(const ir::IRSaturatingSubtractOp *op);
    InterpInstr CompileOp(const ir::IRMultiplyOp *op);
    InterpInstr CompileOp(const ir::IRMultiplyLongOp *op);
    InterpInstr CompileOp(const ir::IRAddLongOp *op);
    InterpInstr CompileOp(const ir::IRStoreFlagsOp *op);
    InterpInstr CompileOp(const ir::IRLoadFlagsOp *op);
    InterpInstr CompileOp(const ir::IRLoadStickyOverflowOp *op);
    InterpInstr CompileOp(const ir::IRBranchOp *op);
    InterpInstr CompileOp(const ir::IRBranchExchangeOp *op);
    InterpInstr CompileOp(const ir::IRLoadCopRegisterOp *op);
    InterpInstr CompileOp(const ir::IRStoreCopRegisterOp *op);
    InterpInstr CompileOp(const ir::IRConstantOp *op);
    InterpInstr CompileOp(const ir::IRCopyVarOp *op);
    InterpInstr CompileOp(const ir::IRGetBaseVectorAddressOp *op);

    // -------------------------------------------------------------------------------------------
    // IR opcode interpreter handlers

    void HandleGetRegister(const Op &varOp, LocationRef loc);
    void HandleSetRegister(const Op &varOp, LocationRef loc);
    void HandleGetCPSR(const Op &varOp, LocationRef loc);
    void HandleSetCPSR(const Op &varOp, LocationRef loc);
    void HandleGetSPSR(const Op &varOp, LocationRef loc);
    void HandleSetSPSR(const Op &varOp, LocationRef loc);
    void HandleMemRead(const Op &varOp, LocationRef loc);
    void HandleMemWrite(const Op &varOp, LocationRef loc);
    void HandlePreload(const Op &varOp, LocationRef loc);
    void HandleLogicalShiftLeft(const Op &varOp, LocationRef loc);
    void HandleLogicalShiftRight(const Op &varOp, LocationRef loc);
    void HandleArithmeticShiftRight(const Op &varOp, LocationRef loc);
    void HandleRotateRight(const Op &varOp, LocationRef loc);
    void HandleRotateRightExtended(const Op &varOp, LocationRef loc);
    void HandleBitwiseAnd(const Op &varOp, LocationRef loc);
    void HandleBitwiseOr(const Op &varOp, LocationRef loc);
    void HandleBitwiseXor(const Op &varOp, LocationRef loc);
    void HandleBitClear(const Op &varOp, LocationRef loc);
    void HandleCountLeadingZeros(const Op &varOp, LocationRef loc);
    void HandleAdd(const Op &varOp, LocationRef loc);
    void HandleAddCarry(const Op &varOp, LocationRef loc);
    void HandleSubtract(const Op &varOp, LocationRef loc);
    void HandleSubtractCarry(const Op &varOp, LocationRef loc);
    void HandleMove(const Op &varOp, LocationRef loc);
    void HandleMoveNegated(const Op &varOp, LocationRef loc);
    void HandleSaturatingAdd(const Op &varOp, LocationRef loc);
    void HandleSaturatingSubtract(const Op &varOp, LocationRef loc);
    void HandleMultiply(const Op &varOp, LocationRef loc);
    void HandleMultiplyLong(const Op &varOp, LocationRef loc);
    void HandleAddLong(const Op &varOp, LocationRef loc);
    void HandleStoreFlags(const Op &varOp, LocationRef loc);
    void HandleLoadFlags(const Op &varOp, LocationRef loc);
    void HandleLoadStickyOverflow(const Op &varOp, LocationRef loc);
    void HandleBranch(const Op &varOp, LocationRef loc);
    void HandleBranchExchange(const Op &varOp, LocationRef loc);
    void HandleLoadCopRegister(const Op &varOp, LocationRef loc);
    void HandleStoreCopRegister(const Op &varOp, LocationRef loc);
    void HandleConstant(const Op &varOp, LocationRef loc);
    void HandleCopyVar(const Op &varOp, LocationRef loc);
    void HandleGetBaseVectorAddress(const Op &varOp, LocationRef loc);
};

} // namespace armajitto::interp
