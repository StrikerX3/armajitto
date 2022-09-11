#pragma once

#include "advanDS/core/clock.hpp"
#include "advanDS/cpu/cpu_model.hpp"
#include "advanDS/cpu/exec_hook_registry.hpp"
#include "advanDS/debug/debug_context.hpp"
#include "advanDS/snapshot.hpp"
#include "advanDS/sys/memory_interface.hpp"
#include "advanDS/util/inline.hpp"
#include "advanDS/util/sign_extend.hpp"
#include "arm/arithmetic.hpp"
#include "arm/conditions.hpp"
#include "arm/exceptions.hpp"
#include "arm/registers.hpp"
#include "util/constexpr_for.hpp"
#include "util/dynamic_bitmap.hpp"

#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace advanDS::cpu::interp::arm7tdmi {

namespace config {
    // TODO: make these configurable at runtime

    // Translate memory addresses to canonical addresses. This allows the cache to reuse blocks in mirrored memory
    // areas, which reduces memory usage with a slight cost to performance. Disabling this will increase memory usage in
    // titles that read from multiple mirrored memory areas.
    //
    // false is the default and recommended setting as the majority of titles tend to stick to a single mirrored area.
    // true may reduce memory usage in a few cases at the cost of performance on all titles.
    inline constexpr bool translateAddressesInCachedExecutor = false;

    // Use calculated memory access timings from the memory interface. Disabling this will force all memory accesses to
    // have the timing specified by fixedAccessTiming.
    //
    // true is the default and recommended setting for compatibility and accuracy.
    // false improves performance.
    inline constexpr bool useMemoryInterfaceAccessTimings = true;

    // Number of cycles per memory access when not using access timings from the memory interface.
    //
    // 1 is the default and recommended setting, as it results in similar (but not 100% accurate) timings to hardware
    // with the usual cache settings.
    inline constexpr core::cycles_t fixedAccessTiming = 1;

} // namespace config

// --- Forward declarations ------------------------------------------------------------------------

template <typename MI, typename Exec>
class ARM7TDMI;

template <typename MI, typename Exec>
using ARMInstructionHandler = core::cycles_t (*)(ARM7TDMI<MI, Exec> &, uint32_t);

template <typename MI, typename Exec>
using THUMBInstructionHandler = core::cycles_t (*)(ARM7TDMI<MI, Exec> &, uint16_t);

// --- Executor declarations -----------------------------------------------------------------------

// Executor interface
class Executor {
public:
    virtual void Reset() = 0;

    virtual core::cycles_t Run(bool enableExecHooks, bool debug, bool singleStep) = 0;

    virtual void FillPipeline() = 0;
    virtual void ReloadPipelineARM() = 0;
    virtual void ReloadPipelineTHUMB() = 0;

    virtual void Stall() = 0;

    virtual void HitBreakpoint() = 0;

    virtual void ChangeExecState(arm::ExecState execState) = 0;

    virtual void ClearCache() = 0;
    virtual void InvalidateCache() = 0;
    virtual void InvalidateCacheAddress(uint32_t address) = 0;
    virtual void InvalidateCacheRange(uint32_t start, uint32_t end) = 0;

    virtual uint32_t GetPipelineFetchSlotOpcode() = 0;
    virtual uint32_t GetPipelineDecodeSlotOpcode() = 0;

    hooks::ExecHookRegistry execHooks;
};

// Interprets every instruction individually for maximum accuracy at the cost of performance.
template <typename MI>
class UncachedExecutor final : public Executor {
public:
    UncachedExecutor(ARM7TDMI<MI, UncachedExecutor<MI>> &arm)
        : m_arm(arm) {}

    void Reset() final;

    core::cycles_t Run(bool enableExecHooks, bool debug, bool singleStep) final;

    void FillPipeline() final;
    void ReloadPipelineARM() final;
    void ReloadPipelineTHUMB() final;

    void Stall() final;

    void HitBreakpoint() final;

    void ChangeExecState(arm::ExecState execState) final;

    void ClearCache() final;
    void InvalidateCache() final;
    void InvalidateCacheAddress(uint32_t address) final;
    void InvalidateCacheRange(uint32_t start, uint32_t end) final;

    uint32_t GetPipelineFetchSlotOpcode() final;
    uint32_t GetPipelineDecodeSlotOpcode() final;

private:
    ARM7TDMI<MI, UncachedExecutor<MI>> &m_arm;

    uint32_t m_pipeline[2];
};

// Caches decoded instruction blocks for improved performance at the cost of some accuracy.
template <typename MI>
class CachedExecutor final : public Executor {
public:
    CachedExecutor(ARM7TDMI<MI, CachedExecutor<MI>> &arm)
        : m_arm(arm) {}

    void Reset() final;

    core::cycles_t Run(bool enableExecHooks, bool debug, bool singleStep) final;

    void FillPipeline() final;
    void ReloadPipelineARM() final;
    void ReloadPipelineTHUMB() final;

    void Stall() final;

    void HitBreakpoint() final;

    void ChangeExecState(arm::ExecState execState) final;

    void ClearCache() final;
    void InvalidateCache() final;
    void InvalidateCacheAddress(uint32_t address) final;
    void InvalidateCacheRange(uint32_t start, uint32_t end) final;

    uint32_t GetPipelineFetchSlotOpcode() final;
    uint32_t GetPipelineDecodeSlotOpcode() final;

private:
    ARM7TDMI<MI, CachedExecutor<MI>> &m_arm;

    // Block cache
    // - 2 caches (ARM, Thumb)
    //   - top level is always allocated as std::array
    //   - page level is allocated dynamically and contains a fixed number of blocks
    //   - blocks contain a fixed number of instructions
    // - nullptr page means "not cached"
    // - entire block is decoded at once
    // - executing is as easy as going through the list of instructions starting at an offset and running them until an
    //   exit condition
    // - exit conditions are:
    //   - branching
    //   - end of block
    //   - block invalidation (self-modifying code)

    static constexpr size_t kPageBits = 12;
    static constexpr size_t kBlockBits = 8;
    static constexpr size_t kPageEntryBits = 32 - kPageBits - kBlockBits;

    static constexpr size_t kNumPages = 1 << kPageBits;
    static constexpr size_t kNumBlocks = 1 << kPageEntryBits;
    static constexpr size_t kBlockSize = 1 << kBlockBits;
    static constexpr size_t kARMEntries = kBlockSize / sizeof(uint32_t);
    static constexpr size_t kTHUMBEntries = kBlockSize / sizeof(uint16_t);

    static constexpr uint32_t kPageShift = 32 - kPageBits;
    static constexpr uint32_t kEntryMask = ~0u >> (32 - kPageEntryBits);
    static constexpr uint32_t kAddressMask = ~0u >> (32 - kBlockBits);
    static constexpr uint32_t kARMAddressMask = kAddressMask >> 2;
    static constexpr uint32_t kTHUMBAddressMask = kAddressMask >> 1;

    struct ARMBlock {
        struct Instruction {
            ARMInstructionHandler<MI, CachedExecutor<MI>> handler;
            ARMInstructionHandler<MI, CachedExecutor<MI>> debugHandler;
            uint32_t instr;
        };
        std::array<Instruction, kARMEntries> instrs;
    };

    struct ThumbBlock {
        struct Instruction {
            THUMBInstructionHandler<MI, CachedExecutor<MI>> handler;
            THUMBInstructionHandler<MI, CachedExecutor<MI>> debugHandler;
            uint16_t instr;
        };
        std::array<Instruction, kTHUMBEntries> instrs;
    };

    template <typename T>
    struct CachePage {
        std::array<T, kNumBlocks> blocks;
        std::bitset<kNumBlocks> valid;
        bool pageValid = true;

        T &operator[](size_t index) {
            return blocks[index];
        }
    };

    std::array<std::unique_ptr<CachePage<ARMBlock>>, kNumPages> m_armCache;
    std::array<std::unique_ptr<CachePage<ThumbBlock>>, kNumPages> m_thumbCache;

    // Set to true when starting execution, false when the block must exit
    bool m_cacheValid;

    uint32_t TranslateAddress(uint32_t address);
    ARMBlock &GetCachedARMCode(uint32_t address);
    ThumbBlock &GetCachedThumbCode(uint32_t address);
};

// --- ARM core ------------------------------------------------------------------------------------

// ARM7TDMI CPU emulator
template <typename MI, typename Exec>
class ARM7TDMI final {
    static_assert(std::is_base_of_v<sys::MemoryInterface, MI>, "MI must implement MemoryInterface");
    static_assert(std::is_base_of_v<Executor, Exec>, "Exec must implement Executor");

    using AccessBus = sys::AccessBus;
    using AccessType = sys::AccessType;
    using AccessSize = sys::AccessSize;

public:
    static constexpr Model model = Model::ARM7TDMI;

    ARM7TDMI(MI &mem, debug::DebugContext &debugContext)
        : m_mem(mem)
        , m_debugContext(debugContext)
        , m_debugARM7(debugContext.GetARM7Common())
        , m_exec(*this) {

        Reset();
    }

    void Reset() {
        m_regs.Reset();
        m_spsr = &m_regs.cpsr;
        m_execState = arm::ExecState::Run;
        m_exec.Reset();
        m_lastInstrBreakpointHit = false;
        m_anyBreakpointHit = false;
    }

    void FillPipeline() {
        m_regs.r15 += (m_regs.cpsr.t ? 4 : 8);
        m_exec.FillPipeline();
    }

    void SetExecHook(void *context, uint32_t address, hooks::ExecHookFn fn) {
        m_exec.execHooks.SetHook(context, address, fn);
    }

    void SetExecRangeHook(void *context, uint32_t startAddress, uint32_t endAddress, hooks::ExecHookFn fn) {
        m_exec.execHooks.SetHookRange(context, startAddress, endAddress, fn);
    }

    void ClearExecHooks() {
        m_exec.execHooks.Clear();
    }

    bool IsBreakpointHit() const {
        return m_anyBreakpointHit;
    }

    // Executes one instruction or block
    template <bool enableExecHooks, bool debug, bool singleStep>
    core::cycles_t Run() {
        if constexpr (debug) {
            m_anyBreakpointHit = false;
        }
        return m_exec.Run(enableExecHooks, debug, singleStep);
    }

    // Enters the IRQ exception vector
    core::cycles_t HandleIRQ() {
        if (m_regs.cpsr.i) {
            return 0;
        }
        return EnterException(arm::Excpt_NormalInterrupt);
    }

    void ClearCache() {
        m_exec.ClearCache();
    }

    void InvalidateCache() {
        m_exec.InvalidateCache();
    }

    void InvalidateCacheAddress(uint32_t address) {
        m_exec.InvalidateCacheAddress(address);
    }

    void InvalidateCacheRange(uint32_t start, uint32_t end) {
        m_exec.InvalidateCacheRange(start, end);
    }

    const arm::Registers &GetRegisters() const {
        return m_regs;
    }

    arm::Registers &GetRegisters() {
        return m_regs;
    }

    std::optional<arm::PSR> GetSPSR() const {
        if (m_spsr != &m_regs.cpsr) {
            return *m_spsr;
        } else {
            return std::nullopt;
        }
    }

    bool SetSPSR(PSR psr) {
        if (m_spsr != &m_regs.cpsr) {
            m_spsr->u32 = psr.u32;
            return true;
        } else {
            return false;
        }
    }

    uint32_t GetLastExecutedPC() const {
        return m_regs.r15 - (m_regs.cpsr.t ? 4 : 8);
    }

    arm::ExecState GetExecState() const {
        return m_execState;
    }

    void SetExecState(arm::ExecState execState) {
        m_execState = execState;
        m_exec.ChangeExecState(execState);
    }

    core::cycles_t GetAccessCycles(uint32_t address, sys::AccessBus bus, sys::AccessType type, sys::AccessSize size,
                                   bool /*write*/) {
        return AccessCycles(address, bus, type, size);
    }

    template <typename T>
    T CodeRead(uint32_t address) {
        static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "CodeRead requires uint16_t or uint32_t");
        if constexpr (std::is_same_v<T, uint16_t>) {
            return CodePeekHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return CodePeekWord(address);
        }
    }

    template <typename T>
    T DataRead(uint32_t address) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataRead requires uint8_t, uint16_t or uint32_t");
        if constexpr (std::is_same_v<T, uint8_t>) {
            return DataPeekByte(address);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return DataPeekHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return DataPeekWord(address);
        }
    }

    template <typename T>
    void DataWrite(uint32_t address, T value) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataWrite requires uint8_t, uint16_t or uint32_t");
        if constexpr (std::is_same_v<T, uint8_t>) {
            DataPokeByte(address, value);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            DataPokeHalf(address, value);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            DataPokeWord(address, value);
        }
    }

    uint32_t GetPipelineFetchSlotOpcode() {
        return m_exec.GetPipelineFetchSlotOpcode();
    }

    uint32_t GetPipelineDecodeSlotOpcode() {
        return m_exec.GetPipelineDecodeSlotOpcode();
    }

    void Stall() {
        m_exec.Stall();
    }

    void SetMode(arm::Mode newMode) {
        arm::Mode oldMode = m_regs.cpsr.mode;
        arm::Bank oldBank = GetBankFromMode(oldMode);
        arm::Bank newBank = GetBankFromMode(newMode);

        // Update spsr reference
        if (newBank == arm::Bank_User) {
            m_spsr = &m_regs.cpsr;
        } else {
            m_spsr = &m_regs.spsr[newBank];
        }

        if (oldMode == newMode) {
            return;
        }

        m_regs.cpsr.mode = newMode;

        // Swap R8-R12 only if we're entering or leaving FIQ
        if (oldBank == arm::Bank_FIQ || newBank == arm::Bank_FIQ) {
            arm::Bank oldBankFIQ = (oldMode == arm::Mode::FIQ) ? arm::Bank_FIQ : arm::Bank_User;
            arm::Bank newBankFIQ = (newMode == arm::Mode::FIQ) ? arm::Bank_FIQ : arm::Bank_User;
            for (size_t i = 8; i <= 12; i++) {
                m_regs.bankregs[oldBankFIQ][i - 8] = m_regs.regs[i];
                m_regs.regs[i] = m_regs.bankregs[newBankFIQ][i - 8];
            }
        }

        // Swap R13 and R14
        for (size_t i = 13; i <= 14; i++) {
            m_regs.bankregs[oldBank][i - 8] = m_regs.regs[i];
            m_regs.regs[i] = m_regs.bankregs[newBank][i - 8];
        }
    }

    bool HasCoprocessor(uint8_t) const {
        return false;
    }

    uint32_t CPRead(uint8_t, uint16_t) const {
        return 0;
    }

    void CPWrite(uint8_t, uint16_t, uint32_t) {}

    void StoreCoprocessorsSnapshot(snapshot::CP15 &snapshot) const {
        // No coprocessors
    }

    void LoadCoprocessorsSnapshot(const snapshot::CP15 &snapshot) {
        // No coprocessors
    }

    void FinishSnapshotLoad() {
        // Update SPSR reference
        arm::Bank bank = GetBankFromMode(m_regs.cpsr.mode);
        if (bank == arm::Bank_User) {
            m_spsr = &m_regs.cpsr;
        } else {
            m_spsr = &m_regs.spsr[bank];
        }

        m_exec.ClearCache();
        m_exec.FillPipeline();
    }

private:
    arm::Registers m_regs;
    MI &m_mem;
    debug::DebugContext &m_debugContext;
    debug::ARMCommon &m_debugARM7;
    Exec m_exec;
    arm::PSR *m_spsr;
    arm::ExecState m_execState;

    static constexpr uint32_t s_baseAddress = 0x00000000;

    // --- Debugger -----------------------------------------------------------

    static constexpr auto kDebugCPU = debug::CPU::ARM7;

    debug::InstructionBreakpointInfo m_lastInstrBreakpointInfo;
    bool m_lastInstrBreakpointHit;

    bool m_anyBreakpointHit;

    bool IsInstructionBreakpointHit(uint32_t address) {
        return m_debugARM7.GetInstructionBreakpointMap().Test(address >> 1);
    }

    bool IsMemoryBreakpointHit(util::DynamicBitmap<uint32_t, 32, 16> &map, uint32_t address, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            if (map.Test(address + i)) {
                return true;
            }
        }
        return false;
    }

    bool RecordBreakpoint(debug::InstructionBreakpointInfo &&info) {
        if (!m_lastInstrBreakpointHit || m_lastInstrBreakpointInfo != info) {
            m_lastInstrBreakpointInfo = info;
            m_lastInstrBreakpointHit = true;
            m_debugContext.InvokeInstructionBreakpointCallback(info);
            m_exec.HitBreakpoint();
            m_anyBreakpointHit = true;
            return true;
        }
        m_lastInstrBreakpointHit = false;
        return false;
    }

    void RecordBreakpoint(debug::MemoryBreakpointInfo &&info) {
        m_debugContext.InvokeMemoryBreakpointCallback(info);
        m_exec.HitBreakpoint();
        m_anyBreakpointHit = true;
    }

    bool CheckInstructionBreakpoint(uint32_t address, uint32_t instr) {
        if (IsInstructionBreakpointHit(address)) {
            if (m_regs.cpsr.t) {
                return RecordBreakpoint(debug::InstructionBreakpointInfo::Thumb(kDebugCPU, address, instr));
            } else {
                return RecordBreakpoint(debug::InstructionBreakpointInfo::ARM(kDebugCPU, address, instr));
            }
        }
        return false;
    }

    template <bool write, typename T>
    void CheckMemoryBreakpoint(uint32_t address, T value) {
        auto &map = write ? m_debugARM7.GetMemoryWriteBreakpointMap() : m_debugARM7.GetMemoryReadBreakpointMap();
        if (IsMemoryBreakpointHit(map, address, sizeof(T))) {
            static constexpr auto size = std::is_same_v<T, uint8_t>    ? debug::MemoryBreakpointInfo::Size::Byte
                                         : std::is_same_v<T, uint16_t> ? debug::MemoryBreakpointInfo::Size::Half
                                                                       : debug::MemoryBreakpointInfo::Size::Word;
            if constexpr (write) {
                RecordBreakpoint(debug::MemoryBreakpointInfo::Write(kDebugCPU, address, size, value));
            } else {
                RecordBreakpoint(debug::MemoryBreakpointInfo::Read(kDebugCPU, address, size, value));
            }
        }
    }

    // --- Helpers ------------------------------------------------------------

    core::cycles_t EnterException(arm::ExceptionVector vector) {
        const auto &vectorInfo = arm::kExceptionVectorInfos[vector];
        const auto modeBank = GetBankFromMode(vectorInfo.mode);

        const auto nn = m_regs.cpsr.t ? vectorInfo.thumbOffset : vectorInfo.armOffset;
        const auto pc = m_regs.r15 - (m_regs.cpsr.t ? 4 : 8);

        m_regs.spsr[modeBank] = m_regs.cpsr;
        SetMode(vectorInfo.mode);
        m_regs.cpsr.t = 0;
        m_regs.cpsr.i = 1;
        if (vectorInfo.F) {
            m_regs.cpsr.f = 1;
        }

        m_regs.r14 = pc + nn;
        m_regs.r15 = s_baseAddress + vector * 4;
        return ReloadPipelineARM();
    }

    core::cycles_t BranchAndExchange(uint32_t address) {
        bool thumb = address & 1;
        m_regs.cpsr.t = thumb;
        m_regs.r15 = address & (thumb ? ~1 : ~3);
        return thumb ? ReloadPipelineTHUMB() : ReloadPipelineARM();
    }

    core::cycles_t ReloadPipelineARM() {
        assert(m_regs.cpsr.t == 0);
        m_exec.ReloadPipelineARM();
        core::cycles_t cycles = AccessCyclesCNW(m_regs.r15 + 0) + //
                                AccessCyclesCSW(m_regs.r15 + 4) + //
                                AccessCyclesCSW(m_regs.r15 + 8);
        m_regs.r15 += 8;
        return cycles;
    }

    core::cycles_t ReloadPipelineTHUMB() {
        assert(m_regs.cpsr.t == 1);
        m_exec.ReloadPipelineTHUMB();
        core::cycles_t cycles = AccessCyclesCNH(m_regs.r15 + 0) + //
                                AccessCyclesCSH(m_regs.r15 + 2) + //
                                AccessCyclesCSH(m_regs.r15 + 4);
        m_regs.r15 += 4;
        return cycles;
    }

    bool EvalCondition(const arm::ConditionFlags cond) const {
        if (cond == arm::Cond_AL) {
            return true;
        }
        return s_conditionsTable[(m_regs.cpsr.u32 >> 28) | (cond << 4)];
    }

    template <bool checkNegatives>
    core::cycles_t CalcMultiplierCycles(uint32_t multiplier) {
        uint32_t mask = 0xFFFFFF00;
        core::cycles_t cycles = 1;
        do {
            multiplier &= mask;
            if (multiplier == 0) {
                break;
            }
            if constexpr (checkNegatives) {
                if (multiplier == mask) {
                    break;
                }
            }
            mask <<= 8;
            cycles++;
        } while (mask != 0);
        return cycles;
    }

    ALWAYS_INLINE uint32_t Shift(uint32_t value, uint8_t shiftOp, bool &carry, core::cycles_t &cycles) {
        uint8_t type = (shiftOp >> 1) & 0b11;
        uint8_t amount;
        bool imm = (shiftOp & 1) == 0;
        if (imm) {
            // Immediate shift
            amount = (shiftOp >> 3) & 0b11111;
        } else {
            // Register shift
            uint8_t reg = (shiftOp >> 4) & 0b1111;
            amount = m_regs.regs[reg];
            cycles++; // 1I if using register specified shift
        }
        switch (type) {
        case 0b00: return arm::LSL(value, amount, carry);
        case 0b01: return arm::LSR(value, amount, carry, imm);
        case 0b10: return arm::ASR(value, amount, carry, imm);
        case 0b11: return arm::ROR(value, amount, carry, imm);
        }
        return value;
    }

    ALWAYS_INLINE uint32_t Shift(uint32_t value, uint8_t shiftOp, core::cycles_t &cycles) {
        bool carry = m_regs.cpsr.c;
        return Shift(value, shiftOp, carry, cycles);
    }

    // --- Memory accessors ---------------------------------------------------

    template <typename T, bool peek>
    T CodeReadImpl(uint32_t address) {
        static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "CodeReadImpl requires uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        if constexpr (std::is_same_v<T, uint16_t>) {
            return peek ? m_mem.PeekHalf(address) : m_mem.ReadHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return peek ? m_mem.PeekWord(address) : m_mem.ReadWord(address);
        }
    }

    uint16_t CodeReadHalf(uint32_t address) {
        return CodeReadImpl<uint16_t, false>(address);
    }

    uint32_t CodeReadWord(uint32_t address) {
        return CodeReadImpl<uint32_t, false>(address);
    }

    uint16_t CodePeekHalf(uint32_t address) {
        return CodeReadImpl<uint16_t, true>(address);
    }

    uint32_t CodePeekWord(uint32_t address) {
        return CodeReadImpl<uint32_t, true>(address);
    }

    template <typename T, bool peek, bool debug>
    T DataReadImpl(uint32_t address) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataReadImpl requires uint8_t, uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        T value;
        if constexpr (std::is_same_v<T, uint8_t>) {
            value = peek ? m_mem.PeekByte(address) : m_mem.ReadByte(address);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            value = peek ? m_mem.PeekHalf(address) : m_mem.ReadHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            value = peek ? m_mem.PeekWord(address) : m_mem.ReadWord(address);
        }
        if constexpr (debug) {
            CheckMemoryBreakpoint<false>(address, value);
        }
        return value;
    }

    template <bool debug>
    uint8_t DataReadByte(uint32_t address) {
        return DataReadImpl<uint8_t, false, debug>(address);
    }

    template <bool debug>
    uint16_t DataReadHalf(uint32_t address) {
        return DataReadImpl<uint16_t, false, debug>(address);
    }

    template <bool debug>
    uint32_t DataReadWord(uint32_t address) {
        return DataReadImpl<uint32_t, false, debug>(address);
    }

    uint8_t DataPeekByte(uint32_t address) {
        return DataReadImpl<uint8_t, true, false>(address);
    }

    uint16_t DataPeekHalf(uint32_t address) {
        return DataReadImpl<uint16_t, true, false>(address);
    }

    uint32_t DataPeekWord(uint32_t address) {
        return DataReadImpl<uint32_t, true, false>(address);
    }

    template <typename T, bool poke, bool debug>
    void DataWriteImpl(uint32_t address, T value) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataWriteImpl requires uint8_t, uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        if constexpr (debug) {
            CheckMemoryBreakpoint<true>(address, value);
        }
        if constexpr (std::is_same_v<T, uint8_t>) {
            if constexpr (poke) {
                m_mem.PokeByte(address, value);
            } else {
                m_mem.WriteByte(address, value);
            }
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if constexpr (poke) {
                m_mem.PokeHalf(address, value);
            } else {
                m_mem.WriteHalf(address, value);
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if constexpr (poke) {
                m_mem.PokeWord(address, value);
            } else {
                m_mem.WriteWord(address, value);
            }
        }
    }

    template <bool debug>
    void DataWriteByte(uint32_t address, uint8_t value) {
        DataWriteImpl<uint8_t, false, debug>(address, value);
    }

    template <bool debug>
    void DataWriteHalf(uint32_t address, uint16_t value) {
        DataWriteImpl<uint16_t, false, debug>(address, value);
    }

    template <bool debug>
    void DataWriteWord(uint32_t address, uint32_t value) {
        DataWriteImpl<uint32_t, false, debug>(address, value);
    }

    void DataPokeByte(uint32_t address, uint8_t value) {
        DataWriteImpl<uint8_t, true, false>(address, value);
    }

    void DataPokeHalf(uint32_t address, uint16_t value) {
        DataWriteImpl<uint16_t, true, false>(address, value);
    }

    void DataPokeWord(uint32_t address, uint32_t value) {
        DataWriteImpl<uint32_t, true, false>(address, value);
    }

    template <bool debug>
    int32_t DataReadSignedByte(uint32_t address) {
        return util::SignExtend<8, int32_t>(DataReadByte<debug>(address));
    }

    template <bool debug>
    int32_t DataReadSignedHalf(uint32_t address) {
        if (address & 1) {
            return util::SignExtend<8, int32_t>(DataReadByte<debug>(address));
        }
        return util::SignExtend<16, int32_t>(DataReadHalf<debug>(address));
    }

    template <bool debug>
    uint32_t DataReadUnalignedHalf(uint32_t address) {
        uint16_t value = DataReadHalf<debug>(address);
        if (address & 1) {
            return std::rotr(value, 8);
        }
        return value;
    }

    template <bool debug>
    uint32_t DataReadUnalignedWord(uint32_t address) {
        uint32_t value = DataReadWord<debug>(address);
        uint32_t offset = (address & 3) * 8;
        return std::rotr(value, offset);
    }

    // --- Memory timing helpers ----------------------------------------------

    core::cycles_t AccessCycles(uint32_t address, AccessBus bus, AccessType type, AccessSize size) {
        if constexpr (config::useMemoryInterfaceAccessTimings) {
            return m_mem.AccessCycles(address, bus, type, size);
        } else {
            return config::fixedAccessTiming;
        }
    }

    core::cycles_t AccessCyclesCSH(uint32_t address) {
        return AccessCycles(address, AccessBus::Code, AccessType::Sequential, AccessSize::Half);
    }
    core::cycles_t AccessCyclesCSW(uint32_t address) {
        return AccessCycles(address, AccessBus::Code, AccessType::Sequential, AccessSize::Word);
    }

    core::cycles_t AccessCyclesCNH(uint32_t address) {
        return AccessCycles(address, AccessBus::Code, AccessType::NonSequential, AccessSize::Half);
    }
    core::cycles_t AccessCyclesCNW(uint32_t address) {
        return AccessCycles(address, AccessBus::Code, AccessType::NonSequential, AccessSize::Word);
    }

    core::cycles_t AccessCyclesDSW(uint32_t address) {
        return AccessCycles(address, AccessBus::Data, AccessType::Sequential, AccessSize::Word);
    }

    core::cycles_t AccessCyclesDNB(uint32_t address) {
        return AccessCycles(address, AccessBus::Data, AccessType::NonSequential, AccessSize::Byte);
    }
    core::cycles_t AccessCyclesDNH(uint32_t address) {
        return AccessCycles(address, AccessBus::Data, AccessType::NonSequential, AccessSize::Half);
    }
    core::cycles_t AccessCyclesDNW(uint32_t address) {
        return AccessCycles(address, AccessBus::Data, AccessType::NonSequential, AccessSize::Word);
    }

    // --- ARM instruction handlers -------------------------------------------

    core::cycles_t _ARM_BranchAndExchange(uint32_t instr) {
        uint8_t rn = instr & 0xF;
        uint32_t value = m_regs.regs[rn];
        return BranchAndExchange(value);
    }

    template <bool l>
    core::cycles_t _ARM_BranchAndBranchWithLink(uint32_t instr) {
        uint32_t value = util::SignExtend<24>(instr & 0xFFFFFF) << 2;
        if constexpr (l) {
            m_regs.r14 = m_regs.r15 - 4;
        }
        m_regs.r15 += value;
        return ReloadPipelineARM();
    }

    template <bool i, uint8_t opcode, bool s>
    core::cycles_t _ARM_DataProcessing(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0xF;
        uint8_t rd = (instr >> 12) & 0xF;

        core::cycles_t cycles = 0;

        auto op1 = m_regs.regs[rn];
        auto &dst = m_regs.regs[rd];
        uint32_t op2;
        bool carry = m_regs.cpsr.c;
        if constexpr (i) {
            uint8_t rotate = (instr >> 8) & 0xF;
            uint8_t imm = (instr >> 0) & 0xFF;
            op2 = arm::RotateImm(imm, rotate, carry);
        } else {
            uint8_t shift = (instr >> 4) & 0xFF;
            uint8_t rm = (instr >> 0) & 0xF;
            uint32_t value = m_regs.regs[rm];
            if (shift & 1) {
                if (rm == 15) {
                    value += 4;
                }
                if (rn == 15) {
                    op1 += 4;
                }
            }
            op2 = Shift(value, shift, carry, cycles);
        }

        if constexpr (s) {
            if (rd == 15) {
                auto spsr = m_spsr;
                SetMode(spsr->mode);
                m_regs.cpsr.u32 = spsr->u32;
            }
        }

        uint32_t result;
        bool overflow = m_regs.cpsr.v;
        if constexpr (opcode == 0b0000) {
            // AND
            result = dst = op1 & op2;
        } else if constexpr (opcode == 0b0001) {
            // EOR
            result = dst = op1 ^ op2;
        } else if constexpr (opcode == 0b0010) {
            // SUB
            result = dst = arm::SUB(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b0011) {
            // RSB
            result = dst = arm::SUB(op2, op1, carry, overflow);
        } else if constexpr (opcode == 0b0100) {
            // ADD
            result = dst = arm::ADD(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b0101) {
            // ADC
            carry = m_regs.cpsr.c;
            result = dst = arm::ADC(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b0110) {
            // SBC
            carry = m_regs.cpsr.c;
            result = dst = arm::SBC(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b0111) {
            // RSC
            carry = m_regs.cpsr.c;
            result = dst = arm::SBC(op2, op1, carry, overflow);
        } else if constexpr (opcode == 0b1000) {
            // TST
            result = op1 & op2;
        } else if constexpr (opcode == 0b1001) {
            // TEQ
            result = op1 ^ op2;
        } else if constexpr (opcode == 0b1010) {
            // CMP
            result = arm::SUB(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b1011) {
            // CMN
            result = arm::ADD(op1, op2, carry, overflow);
        } else if constexpr (opcode == 0b1100) {
            // ORR
            result = dst = op1 | op2;
        } else if constexpr (opcode == 0b1101) {
            // MOV
            result = dst = op2;
        } else if constexpr (opcode == 0b1110) {
            // BIC
            result = dst = op1 & ~op2;
        } else if constexpr (opcode == 0b1111) {
            // MVN
            result = dst = ~op2;
        }

        if constexpr (s) {
            if (rd != 15) {
                m_regs.cpsr.z = (result == 0);
                m_regs.cpsr.n = (result >> 31);
                m_regs.cpsr.c = carry;
                m_regs.cpsr.v = overflow;
            }
        }

        if (rd == 15) {
            if constexpr (s) {
                cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
            } else {
                cycles += ReloadPipelineARM();
            }
        } else {
            m_regs.r15 += 4;
            cycles += AccessCyclesCSW(m_regs.r15); // 1S to fetch next instruction
        }
        return cycles;
    }

    // PSR Transfer to register
    template <bool ps>
    core::cycles_t _ARM_MRS(uint32_t instr) {
        uint8_t rd = (instr >> 12) & 0b1111;

        if constexpr (ps) {
            m_regs.regs[rd] = m_spsr->u32;
        } else {
            m_regs.regs[rd] = m_regs.cpsr.u32;
        }

        m_regs.r15 += 4;
        return AccessCyclesCSW(m_regs.r15); // 1S to fetch next instruction
    }

    // PSR Transfer register or immediate value to PSR
    template <bool i, bool pd>
    core::cycles_t _ARM_MSR(uint32_t instr) {
        uint32_t value;
        if constexpr (i) {
            uint8_t imm = (instr & 0xFF);
            uint8_t rotate = (instr >> 8) & 0xF;
            value = arm::RotateImm(imm, rotate);
        } else {
            uint8_t rm = (instr & 0b1111);
            value = m_regs.regs[rm];
        }

        uint32_t mask = 0;
        if ((instr >> 19) & 1) {
            mask |= 0xFF000000;
        }
        if ((instr >> 18) & 1) {
            mask |= 0x00FF0000;
        }
        if ((instr >> 17) & 1) {
            mask |= 0x0000FF00;
        }
        if ((instr >> 16) & 1) {
            mask |= 0x000000FF;
        }
        value &= mask;

        if constexpr (pd) {
            // Write to SPSR, but only if not pointing to CPSR
            if (m_spsr != &m_regs.cpsr) {
                m_spsr->u32 = (m_spsr->u32 & ~mask) | value;
            }
        } else {
            // Write to CPSR, updating mode if necessary
            if ((instr >> 16) & 1) {
                SetMode(static_cast<arm::Mode>(value & 0x1F));
            }
            m_regs.cpsr.u32 = (m_regs.cpsr.u32 & ~mask) | value;
        }

        m_regs.r15 += 4;
        return AccessCyclesCSW(m_regs.r15); // 1S to fetch next instruction
    }

    template <bool a, bool s>
    core::cycles_t _ARM_MultiplyAccumulate(uint32_t instr) {
        uint8_t rd = (instr >> 16) & 0b1111;
        uint8_t rn = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t multiplier = m_regs.regs[rs];

        uint32_t result = m_regs.regs[rm] * multiplier;
        if constexpr (a) {
            result += m_regs.regs[rn];
        }
        m_regs.regs[rd] = result;

        if constexpr (s) {
            m_regs.cpsr.z = (result == 0);
            m_regs.cpsr.n = (result >> 31);
        }

        core::cycles_t cycles = CalcMultiplierCycles<true>(multiplier); // mI cycles depending on the multiplier's bytes
        if constexpr (a) {
            cycles++; // 1I for accumulate operation
        }
        cycles += AccessCyclesCNW(m_regs.r15 + 4); // 1N (merged I-S?) to fetch next instruction

        m_regs.r15 += 4;
        return cycles;
    }

    template <bool u, bool a, bool s>
    core::cycles_t _ARM_MultiplyAccumulateLong(uint32_t instr) {
        uint8_t rdHi = (instr >> 16) & 0b1111;
        uint8_t rdLo = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t multiplier = m_regs.regs[rs];

        int64_t result;
        if constexpr (u) {
            int64_t multiplicand = util::SignExtend<32, int64_t>(m_regs.regs[rm]);
            int64_t signedMultiplier = util::SignExtend<32, int64_t>(multiplier);
            result = multiplicand * signedMultiplier;
            if constexpr (a) {
                int64_t value = (uint64_t)m_regs.regs[rdLo] | ((uint64_t)m_regs.regs[rdHi] << 32ull);
                result += value;
            }
        } else {
            uint64_t unsignedResult = (uint64_t)m_regs.regs[rm] * (uint64_t)multiplier;
            result = static_cast<int64_t>(unsignedResult);
            if constexpr (a) {
                uint64_t value = (uint64_t)m_regs.regs[rdLo] | ((uint64_t)m_regs.regs[rdHi] << 32ull);
                result += value;
            }
        }

        m_regs.regs[rdLo] = result;
        m_regs.regs[rdHi] = result >> 32ull;

        if constexpr (s) {
            m_regs.cpsr.z = (result == 0);
            m_regs.cpsr.n = (result >> 63ull);
        }

        m_regs.r15 += 4;
        core::cycles_t cycles = 1 + CalcMultiplierCycles<u>(multiplier); // (1 + m)I cycles based on the multiplier
        if constexpr (a) {
            cycles++; // 1I for accumulate operation
        }
        cycles += AccessCyclesCNW(m_regs.r15); // 1N (merged I-S?) to fetch next instruction
        return cycles;
    }

    template <bool i, bool p, bool u, bool b, bool w, bool l, bool debug>
    core::cycles_t _ARM_SingleDataTransfer(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint16_t offset = (instr & 0xFFF);

        // When the W bit is set in a post-indexed operation, the transfer happens in non-privileged user mode
        arm::Mode origMode;
        if constexpr (w && !p) {
            origMode = m_regs.cpsr.mode;
            SetMode(arm::Mode::User);
        }

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Compute address
        uint32_t offsetValue;
        if constexpr (i) {
            uint8_t rm = (offset & 0b1111);
            uint8_t shift = (offset >> 4);
            bool carry = m_regs.cpsr.c;
            offsetValue = Shift(m_regs.regs[rm], shift, carry, cycles);
        } else {
            offsetValue = offset;
        }
        uint32_t address = m_regs.regs[rn];
        if constexpr (p) {
            address += (u ? offsetValue : -offsetValue);
        }

        // Perform the transfer
        auto &dst = m_regs.regs[rd];
        if constexpr (b && l) {
            // LDRB
            cycles += AccessCyclesDNB(address);
            dst = DataReadByte<debug>(address);
        } else if constexpr (b) {
            // STRB
            cycles += AccessCyclesDNB(address);
            DataWriteByte<debug>(address, dst + ((rd == 15) ? 4 : 0));
        } else if constexpr (l) {
            // LDR
            cycles += AccessCyclesDNW(address);
            dst = DataReadUnalignedWord<debug>(address);
        } else {
            // STR
            cycles += AccessCyclesDNW(address);
            DataWriteWord<debug>(address, dst + ((rd == 15) ? 4 : 0));
        }

        // Revert back to previous mode from non-privileged user mode
        if constexpr (w && !p) {
            SetMode(origMode);
        }

        // Write back address if requested
        if (!l || rn != rd) {
            if constexpr (!p) {
                m_regs.regs[rn] += (u ? offsetValue : -offsetValue);
            } else if constexpr (w) {
                m_regs.regs[rn] = address;
            }
        }

        // Update PC
        if ((l && rd == 15) || ((!l || rn != rd) && (!p || w) && (rn == 15))) {
            cycles += ReloadPipelineARM();
        } else {
            m_regs.r15 += 4;
            // According to the ARM7TDMI datasheet:
            //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
            //   destination register, and external memory is unused. This third cycle may normally be merged with
            //   the following prefetch to form one memory N-cycle."
            cycles += AccessCyclesCNW(m_regs.r15); // 1N
        }
        return cycles;
    }

    template <bool p, bool u, bool i, bool w, bool l, bool s, bool h, bool debug>
    core::cycles_t _ARM_HalfwordAndSignedDataTransfer(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t offsetHi = (instr >> 8) & 0b1111;
        uint8_t rmOrOffsetLo = (instr >> 0) & 0b1111;

        // Compute address
        uint32_t offsetValue;
        if constexpr (i) {
            offsetValue = rmOrOffsetLo | (offsetHi << 4);
        } else {
            offsetValue = m_regs.regs[rmOrOffsetLo];
        }
        uint32_t address = m_regs.regs[rn];
        if constexpr (p) {
            address += (u ? offsetValue : -offsetValue);
        }

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Perform the transfer
        auto &dst = m_regs.regs[rd];
        if constexpr (s && h) {
            if constexpr (l) {
                // LDRSH
                cycles += AccessCyclesDNH(address);
                dst = DataReadSignedHalf<debug>(address);
            }
        } else if constexpr (s) {
            if constexpr (l) {
                // LDRSB
                cycles += AccessCyclesDNB(address);
                dst = DataReadSignedByte<debug>(address);
            }
        } else if constexpr (h) {
            if constexpr (l) {
                // LDRH
                cycles += AccessCyclesDNH(address);
                dst = DataReadUnalignedHalf<debug>(address);
            } else {
                // STRH
                uint32_t value = m_regs.regs[rd] + (rd == 15 ? 4 : 0);
                cycles += AccessCyclesDNH(address);
                DataWriteHalf<debug>(address, value);
            }
        }

        // Write back address if requested
        if (!l || rn != rd) {
            if constexpr (!p) {
                m_regs.regs[rn] += (u ? offsetValue : -offsetValue);
            } else if constexpr (w) {
                m_regs.regs[rn] = address;
            }
        }

        // Update PC
        if ((l && rd == 15) || ((!l || rn != rd) && (!p || w) && rn == 15)) {
            cycles += ReloadPipelineARM();
        } else {
            m_regs.r15 += 4;
            // According to the ARM7TDMI datasheet:
            //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
            //   destination register, and external memory is unused. This third cycle may normally be merged with the
            //   following prefetch to form one memory N-cycle."
            cycles += AccessCyclesCNW(m_regs.r15); // 1N
        }
        return cycles;
    }

    template <bool p, bool u, bool s, bool w, bool l, bool debug>
    core::cycles_t _ARM_BlockDataTransfer(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint16_t regList = (instr & 0xFFFF);

        uint32_t address = m_regs.regs[rn];
        bool pcIncluded = regList & (1 << 15);
        bool forceUserMode = s && (!l || !pcIncluded);

        arm::Mode prevMode;
        if (forceUserMode) {
            prevMode = m_regs.cpsr.mode;
            SetMode(arm::Mode::User);
        }

        // Get first register and compute total transfer size
        uint32_t firstReg;
        uint32_t lastReg;
        uint32_t size;
        if (regList == 0) {
            // An empty list results in transferring PC only but incrementing the address as if we had a full list
            regList = (1 << 15);
            pcIncluded = true;
            firstReg = 15;
            lastReg = 15;
            size = 16 * 4;
        } else {
            firstReg = std::countr_zero(regList);
            lastReg = 15 - std::countl_zero(regList);
            size = std::popcount(regList) * 4;
        }

        // Precompute addresses
        uint32_t startAddress = address;
        uint32_t finalAddress = address + (u ? size : -size);
        if constexpr (!u) {
            address -= size;
        }

        // Registers are loaded/stored in asceding order in memory, regardless of pre/post-indexing and direction flags.
        // We can implement a loop that transfers registers without reversing the list by reversing the indexing flag
        // when the direction flag is down (U=0), which can be achieved by comparing both for equality.
        constexpr bool preInc = (p == u);

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Execute transfer
        for (uint32_t i = firstReg; i <= lastReg; i++) {
            if (~regList & (1 << i)) {
                continue;
            }

            if constexpr (preInc) {
                address += 4;
            }

            // Transfer data
            if constexpr (l) {
                m_regs.regs[i] = DataReadWord<debug>(address);
                if (i == 15 && s) {
                    auto &spsr = *m_spsr;
                    SetMode(spsr.mode);
                    m_regs.cpsr.u32 = spsr.u32;
                }
            } else {
                uint32_t value;
                if (!s && i == rn) {
                    value = (i == firstReg) ? startAddress : finalAddress;
                } else if (i == 15) {
                    value = m_regs.r15 + 4;
                } else {
                    value = m_regs.regs[i];
                }
                DataWriteWord<debug>(address, value);
            }
            if (i == firstReg) {
                // 1N for the first access
                cycles += AccessCyclesDNW(address);
            } else {
                // 1S for each subsequent access
                cycles += AccessCyclesDSW(address);
            }

            if constexpr (!preInc) {
                address += 4;
            }
        }

        if (forceUserMode) {
            SetMode(prevMode);
        }

        if constexpr (w) {
            if (!l || (~regList & (1 << rn))) {
                m_regs.regs[rn] = finalAddress;
            }
        }

        // Update PC
        if ((l && pcIncluded) || ((w && (~regList & (1 << rn))) && rn == 15)) {
            cycles += ReloadPipelineARM();
        } else {
            m_regs.r15 += 4;
            // According to the ARM7TDMI datasheet:
            //   "[...]  the final (internal) cycle [normally an S-cycle for load operations] moves the last word to its
            //   destination register. [...]
            //   "The last cycle may be merged with the next instruction prefetch to form a single memory N-cycle."
            cycles += AccessCyclesCNW(m_regs.r15); // 1N
        }
        return cycles;
    }

    template <bool b, bool debug>
    core::cycles_t _ARM_SingleDataSwap(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        auto address = m_regs.regs[rn];
        auto src = m_regs.regs[rm];
        auto &dst = m_regs.regs[rd];

        core::cycles_t cycles = 1; // 1I

        // Perform the swap (2N for the accesses)
        if constexpr (b) {
            uint8_t tmp = DataReadByte<debug>(address);
            DataWriteByte<debug>(address, src);
            if (rd != 15) {
                dst = tmp;
            }
            cycles += AccessCyclesDNB(address) * 2; // 2N
        } else {
            uint32_t tmp = DataReadUnalignedWord<debug>(address);
            DataWriteWord<debug>(address, src);
            if (rd != 15) {
                dst = tmp;
            }
            cycles += AccessCyclesDNW(address) * 2; // 2N
        }

        cycles += AccessCyclesCSW(m_regs.r15 + 4); // 1S to fetch next instruction
        m_regs.r15 += 4;

        return cycles;
    }

    // Software Interrupt
    core::cycles_t _ARM_SoftwareInterrupt(uint32_t instr) {
        // uint32_t comment = (instr & 0xFFFFFF);
        return EnterException(arm::Excpt_SoftwareInterrupt);
    }

    // template <uint8_t cpopc>
    core::cycles_t _ARM_CopDataOperations(uint32_t instr) {
        return EnterException(arm::Excpt_UndefinedInstruction);
    }

    // template <bool p, bool u, bool n, bool w, bool l>
    core::cycles_t _ARM_CopDataTransfer(uint32_t instr) {
        return EnterException(arm::Excpt_UndefinedInstruction);
    }

    // template <uint8_t cpopc, bool s>
    template <bool s>
    core::cycles_t _ARM_CopRegTransfer(uint32_t instr) {
        // uint16_t crn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t cpnum = (instr >> 8) & 0b1111;
        // uint16_t crm = (instr >> 0) & 0b1111;

        if (cpnum == 14) {
            // ARM7TDMI contains a dummy CP14 that responds with the fetched opcode
            if constexpr (s) {
                m_regs.regs[rd] = m_exec.GetPipelineFetchSlotOpcode();
            }
            m_regs.r15 += 4;
            return AccessCyclesCSW(m_regs.r15 + 4); // 1S to fetch next instruction
        } else {
            return EnterException(arm::Excpt_UndefinedInstruction);
        }
    }

    core::cycles_t _ARM_UndefinedInstruction(uint32_t instr) {
        return 1 + EnterException(arm::Excpt_UndefinedInstruction); // 1I + instruction fetch
    }

    core::cycles_t _ARM_Unmapped(uint32_t instr) {
        throw std::runtime_error("Unmapped ARM instruction");
    }

    // --- THUMB instruction handlers -----------------------------------------

    template <uint8_t op, uint8_t offset>
    core::cycles_t _THUMB_MoveShiftedRegister(uint16_t instr) {
        uint8_t rs = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        auto &dst = m_regs.regs[rd];
        bool carry = m_regs.cpsr.c;
        dst = arm::CalcImmShift<static_cast<arm::ShiftOp>(op)>(m_regs.regs[rs], offset, carry);
        m_regs.cpsr.z = (dst == 0);
        m_regs.cpsr.n = (dst >> 31);
        m_regs.cpsr.c = carry;

        m_regs.r15 += 2;
        return 1 + AccessCyclesCSH(m_regs.r15); // 1I + 1S to fetch next instruction
    }

    template <bool i, bool op, uint8_t rnOrOffset>
    core::cycles_t _THUMB_AddSub(uint16_t instr) {
        uint8_t rs = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        uint32_t value = i ? rnOrOffset : m_regs.regs[rnOrOffset];

        auto &src = m_regs.regs[rs];
        auto &dst = m_regs.regs[rd];
        bool carry;
        bool overflow;
        if constexpr (op) {
            dst = arm::SUB(src, value, carry, overflow);
        } else {
            dst = arm::ADD(src, value, carry, overflow);
        }
        m_regs.cpsr.z = (dst == 0);
        m_regs.cpsr.n = (dst >> 31);
        m_regs.cpsr.c = carry;
        m_regs.cpsr.v = overflow;

        m_regs.r15 += 2;
        return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
    }

    template <uint8_t op, uint8_t rd>
    core::cycles_t _THUMB_MovCmpAddSubImmediate(uint16_t instr) {
        uint8_t offset = (instr & 0xFF);

        auto &dst = m_regs.regs[rd];
        bool carry = m_regs.cpsr.c;
        bool overflow = m_regs.cpsr.v;
        uint32_t result;
        if constexpr (op == 0b00) {
            // MOV
            result = dst = offset;
        } else if constexpr (op == 0b01) {
            // CMP
            result = arm::SUB(dst, offset, carry, overflow);
        } else if constexpr (op == 0b10) {
            // ADD
            result = dst = arm::ADD(dst, offset, carry, overflow);
        } else if constexpr (op == 0b11) {
            // SUB
            result = dst = arm::SUB(dst, offset, carry, overflow);
        }

        m_regs.cpsr.z = (result == 0);
        m_regs.cpsr.n = (result >> 31);
        m_regs.cpsr.c = carry;
        m_regs.cpsr.v = overflow;

        m_regs.r15 += 2;
        return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
    }

    template <uint8_t op>
    core::cycles_t _THUMB_ALUOperations(uint16_t instr) {
        uint8_t rs = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        core::cycles_t cycles = 0;

        auto &src = m_regs.regs[rs];
        auto &dst = m_regs.regs[rd];
        uint32_t result;
        bool carry = m_regs.cpsr.c;
        bool overflow = m_regs.cpsr.v;
        if constexpr (op == 0b0000) {
            // AND
            result = dst &= src;
        } else if constexpr (op == 0b0001) {
            // EOR
            result = dst ^= src;
        } else if constexpr (op == 0b0010) {
            // LSL
            result = dst = arm::LSL(dst, src, carry);
            cycles++; // 1I due to use of register specified shift
        } else if constexpr (op == 0b0011) {
            // LSR
            result = dst = arm::LSR(dst, src, carry, false);
            cycles++; // 1I due to use of register specified shift
        } else if constexpr (op == 0b0100) {
            // ASR
            result = dst = arm::ASR(dst, src, carry, false);
            cycles++; // 1I due to use of register specified shift
        } else if constexpr (op == 0b0101) {
            // ADC
            result = dst = arm::ADC(dst, src, carry, overflow);
        } else if constexpr (op == 0b0110) {
            // SBC
            result = dst = arm::SBC(dst, src, carry, overflow);
        } else if constexpr (op == 0b0111) {
            // ROR
            result = dst = arm::ROR(dst, src, carry, false);
            cycles++; // 1I due to use of register specified shift
        } else if constexpr (op == 0b1000) {
            // TST
            result = dst & src;
        } else if constexpr (op == 0b1001) {
            // NEG
            result = dst = arm::SUB(0, src, carry, overflow);
        } else if constexpr (op == 0b1010) {
            // CMP
            result = arm::SUB(dst, src, carry, overflow);
        } else if constexpr (op == 0b1011) {
            // CMN
            result = arm::ADD(dst, src, carry, overflow);
        } else if constexpr (op == 0b1100) {
            // ORR
            result = dst |= src;
        } else if constexpr (op == 0b1101) {
            // MUL
            cycles += CalcMultiplierCycles<true>(dst);
            result = dst *= src;
        } else if constexpr (op == 0b1110) {
            // BIC
            result = dst &= ~src;
        } else if constexpr (op == 0b1111) {
            // MVN
            result = dst = ~src;
        }

        m_regs.cpsr.z = (result == 0);
        m_regs.cpsr.n = (result >> 31);
        m_regs.cpsr.c = carry;
        m_regs.cpsr.v = overflow;

        m_regs.r15 += 2;

        if constexpr ((op == 0b0010) || (op == 0b0011) || (op == 0b0100) || (op == 0b0111) || (op == 0b1101)) {
            cycles += AccessCyclesCNH(m_regs.r15); // 1N (merged I-S?) to fetch next instruction
        } else {
            cycles += AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
        }
        return cycles;
    }

    template <uint8_t op, bool h1, bool h2>
    core::cycles_t _THUMB_HiRegOperations(uint16_t instr) {
        uint8_t rshs = ((instr >> 3) & 0b111) + (h2 ? 8 : 0);
        uint8_t rdhd = ((instr >> 0) & 0b111) + (h1 ? 8 : 0);

        auto &src = m_regs.regs[rshs];
        auto &dst = m_regs.regs[rdhd];
        if constexpr (op == 0b11) {
            // BX
            return BranchAndExchange(src);
        } else {
            if constexpr (op == 0b00) {
                // ADD
                dst += src;
            } else if constexpr (op == 0b01) {
                // CMP
                bool carry;
                bool overflow;
                uint32_t result = arm::SUB(dst, src, carry, overflow);
                m_regs.cpsr.z = (result == 0);
                m_regs.cpsr.n = (result >> 31);
                m_regs.cpsr.c = carry;
                m_regs.cpsr.v = overflow;
            } else if constexpr (op == 0b10) {
                // MOV
                dst = src;
            }

            if (rdhd == 15 && op != 0b01) {
                m_regs.r15 &= ~1;
                return ReloadPipelineTHUMB();
            } else {
                m_regs.r15 += 2;
                return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
            }
        }
    }

    template <uint8_t rd, bool debug>
    core::cycles_t _THUMB_PCRelativeLoad(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;
        uint32_t address = (m_regs.r15 & ~3) + offset;
        m_regs.regs[rd] = DataReadWord<debug>(address);

        m_regs.r15 += 2;
        return AccessCyclesDNW(address) +   // 1N
               1 +                          // 1I
               AccessCyclesCNH(m_regs.r15); // 1N to fetch next instruction
        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
    }

    template <bool l, bool b, uint8_t ro, bool debug>
    core::cycles_t _THUMB_LoadStoreRegOffset(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + m_regs.regs[ro];
        auto &dst = m_regs.regs[rd];
        if constexpr (l && b) {
            // LDRB
            dst = DataReadByte<debug>(address);
            cycles += AccessCyclesDNB(address);
        } else if constexpr (l) {
            // LDR
            dst = DataReadUnalignedWord<debug>(address);
            cycles += AccessCyclesDNW(address);
        } else if constexpr (b) {
            // STRB
            DataWriteByte<debug>(address, dst);
            cycles += AccessCyclesDNB(address);
        } else {
            // STR
            DataWriteWord<debug>(address, dst);
            cycles += AccessCyclesDNW(address);
        }

        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
        cycles += AccessCyclesCNH(m_regs.r15 + 2); // 1N
        m_regs.r15 += 2;
        return cycles;
    }

    template <bool h, bool s, uint8_t ro, bool debug>
    core::cycles_t _THUMB_LoadStoreSignExtended(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        core::cycles_t cycles = (h || s) ? 1 : 0; // 1I for loads

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + m_regs.regs[ro];
        auto &dst = m_regs.regs[rd];
        if constexpr (h && s) {
            // LDSH
            dst = DataReadSignedHalf<debug>(address);
            cycles += AccessCyclesDNH(address);
        } else if constexpr (h) {
            // LDRH
            dst = DataReadUnalignedHalf<debug>(address);
            cycles += AccessCyclesDNH(address);
        } else if constexpr (s) {
            // LDSB
            dst = DataReadSignedByte<debug>(address);
            cycles += AccessCyclesDNB(address);
        } else {
            // STRH
            DataWriteHalf<debug>(address, dst);
            cycles += AccessCyclesDNH(address);
        }

        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
        cycles += AccessCyclesCNH(m_regs.r15 + 2); // 1N
        m_regs.r15 += 2;
        return cycles;
    }

    template <bool b, bool l, uint16_t offset, bool debug>
    core::cycles_t _THUMB_LoadStoreImmOffset(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + offset;
        auto &dst = m_regs.regs[rd];
        if constexpr (b && l) {
            // LDRB
            dst = DataReadByte<debug>(address);
            cycles += AccessCyclesDNB(address);
        } else if constexpr (b) {
            // STRB
            DataWriteByte<debug>(address, dst);
            cycles += AccessCyclesDNB(address);
        } else if constexpr (l) {
            // LDR
            dst = DataReadUnalignedWord<debug>(address);
            cycles += AccessCyclesDNW(address);
        } else {
            // STR
            DataWriteWord<debug>(address, dst);
            cycles += AccessCyclesDNW(address);
        }

        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
        cycles += AccessCyclesCNH(m_regs.r15 + 2); // 1N
        m_regs.r15 += 2;
        return cycles;
    }

    template <bool l, uint16_t offset, bool debug>
    core::cycles_t _THUMB_LoadStoreHalfWord(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + offset;
        auto &dst = m_regs.regs[rd];
        if constexpr (l) {
            // LDRH
            dst = DataReadUnalignedHalf<debug>(address);
        } else {
            // STRH
            DataWriteHalf<debug>(address, dst);
        }

        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
        m_regs.r15 += 2;
        core::cycles_t cycles = l ? 1 : 0;     // 1I for loads
        cycles += AccessCyclesDNH(address);    // 1N for the memory access
        cycles += AccessCyclesCNH(m_regs.r15); // 1N for the merged I-S prefetch cycle
        return cycles;
    }

    template <bool l, uint8_t rd, bool debug>
    core::cycles_t _THUMB_SPRelativeLoadStore(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        // Perform the transfer
        uint32_t address = m_regs.r13 + offset;
        auto &dst = m_regs.regs[rd];
        if constexpr (l) {
            dst = DataReadUnalignedWord<debug>(address);
            cycles += AccessCyclesDNW(address);
        } else {
            DataWriteWord<debug>(address, dst);
            cycles += AccessCyclesDNW(address);
        }

        // According to the ARM7TDMI datasheet:
        //   "During the third cycle [normally an S-cycle for load operations] the data is transferred to the
        //   destination register, and external memory is unused. This third cycle may normally be merged with the
        //   following prefetch to form one memory N-cycle."
        cycles += AccessCyclesCNH(m_regs.r15 + 2); // 1N
        m_regs.r15 += 2;
        return cycles;
    }

    template <bool sp, uint8_t rd>
    core::cycles_t _THUMB_LoadAddress(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;
        m_regs.regs[rd] = (sp ? m_regs.r13 : (m_regs.r15 & ~3)) + offset;

        m_regs.r15 += 2;
        return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
    }

    template <bool s>
    core::cycles_t _THUMB_AddOffsetToSP(uint16_t instr) {
        uint32_t offset = (instr & 0x7F) << 2;
        if constexpr (s) {
            m_regs.r13 -= offset;
        } else {
            m_regs.r13 += offset;
        }

        m_regs.r15 += 2;
        return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
    }

    template <bool l, bool r, bool debug>
    core::cycles_t _THUMB_PushPopRegs(uint16_t instr) {
        uint8_t regList = (instr & 0xFF);
        uint32_t address = m_regs.r13;

        core::cycles_t cycles = l ? 1 : 0; // 1I for loads

        AccessType accessType = AccessType::NonSequential; // First transfer is 1N, subsequent transfers are 1S
        auto tickAccess = [&]() {
            cycles += AccessCycles(address, AccessBus::Data, accessType, AccessSize::Word);
            accessType = AccessType::Sequential;
        };
        auto push = [&](uint32_t value) {
            DataWriteWord<debug>(address, value);
            tickAccess();
            address += 4;
        };
        auto pop = [&]() -> uint32_t {
            uint32_t value = DataReadWord<debug>(address);
            tickAccess();
            address += 4;
            return value;
        };

        if constexpr (l) {
            // Pop registers
            for (uint32_t i = 0; i < 8; i++) {
                if (regList & (1 << i)) {
                    m_regs.regs[i] = pop();
                }
            }

            // Pop PC if requested
            if constexpr (r) {
                m_regs.r15 = pop() & ~1;
                cycles++; // 1I for popping PC
                cycles += ReloadPipelineTHUMB();
            }

            // Update SP
            m_regs.r13 = address;
        } else {
            // Precompute address
            address -= std::popcount(regList) * 4;
            if constexpr (r) {
                address -= 4;
            }

            // Update SP
            m_regs.r13 = address;

            // Push registers
            accessType = AccessType::NonSequential;
            for (uint32_t i = 0; i < 8; i++) {
                if (regList & (1 << i)) {
                    push(m_regs.regs[i]);
                }
            }

            // Push LR if requested
            if constexpr (r) {
                push(m_regs.r14);
            }
        }

        if constexpr (!l || !r) {
            m_regs.r15 += 2;
            // According to the ARM7TDMI datasheet:
            //   "[...]  the final (internal) cycle [normally an S-cycle for load operations] moves the last word to its
            //   destination register. [...]
            //   "The last cycle may be merged with the next instruction prefetch to form a single memory N-cycle."
            cycles += AccessCyclesCNH(m_regs.r15); // 1N
        }
        return cycles;
    }

    template <bool l, uint8_t rb, bool debug>
    core::cycles_t _THUMB_MultipleLoadStore(uint16_t instr) {
        auto address = m_regs.regs[rb];
        uint8_t regList = (instr & 0xFF);

        // Empty lists result in only PC being written but incrementing the address as if all registers were transferred
        if (regList == 0) {
            core::cycles_t cycles = 0;
            if constexpr (l) {
                m_regs.r15 = DataReadWord<debug>(address);
                cycles += AccessCyclesDNW(address);
                cycles += ReloadPipelineTHUMB();
            } else {
                m_regs.r15 += 2;
                DataWriteWord<debug>(address, m_regs.r15);
                cycles += AccessCyclesDNW(address);
                cycles += AccessCyclesCNH(m_regs.r15);
            }
            address += 0x40;
            m_regs.regs[rb] = address;
            return cycles;
        }

        uint8_t firstReg = std::countr_zero(regList);
        uint8_t lastReg = 7 - std::countl_zero(regList);

        core::cycles_t cycles = 0;

        AccessType accessType = AccessType::NonSequential; // First transfer is 1N, subsequent transfers are 1S
        auto tickAccess = [&](uint32_t addr) {
            cycles += AccessCycles(addr, AccessBus::Data, accessType, AccessSize::Word);
            accessType = AccessType::Sequential;
        };

        if constexpr (l) {
            auto load = [&]() -> uint32_t {
                uint32_t value = DataReadWord<debug>(address);
                tickAccess(address);
                address += 4;
                return value;
            };

            for (uint32_t i = firstReg; i <= lastReg; i++) {
                if (regList & (1 << i)) {
                    m_regs.regs[i] = load();
                }
            }

            // Writeback address if the register is not in the list
            if ((regList & (1 << rb)) == 0) {
                m_regs.regs[rb] = address;
            }
        } else {
            auto store = [&](uint32_t value) {
                DataWriteWord<debug>(address, value);
                tickAccess(address);
                address += 4;
            };

            // Precompute final address if rb happens to be in the list but is not the first register
            uint32_t finalAddress = address + std::popcount(regList) * 4;

            // Transfer registers to memory
            for (uint32_t i = firstReg; i <= lastReg; i++) {
                if (regList & (1 << i)) {
                    if (i == rb && rb != firstReg) {
                        store(finalAddress);
                    } else {
                        store(m_regs.regs[i]);
                    }
                }
            }

            // Writeback address
            m_regs.regs[rb] = address;
        }

        m_regs.r15 += 2;

        // According to the ARM7TDMI datasheet:
        //   "[...] the final (internal) cycle [normally an S-cycle for load operations] moves the last word to its
        //   destination register. [...]
        //   "The last cycle may be merged with the next instruction prefetch to form a single memory N-cycle."
        cycles += AccessCyclesCNH(m_regs.r15); // 1N
        return cycles;
    }

    template <arm::ConditionFlags cond>
    core::cycles_t _THUMB_ConditionalBranch(uint16_t instr) {
        if (EvalCondition(cond)) {
            int32_t offset = util::SignExtend<8, int32_t>(instr & 0xFF) << 1;
            m_regs.r15 += offset;
            return ReloadPipelineTHUMB();
        } else {
            m_regs.r15 += 2;
            return AccessCyclesCSH(m_regs.r15); // 1S to fetch next instruction
        }
    }

    core::cycles_t _THUMB_SoftwareInterrupt(uint16_t instr) {
        // uint8_t comment = (instr & 0xFF);
        return EnterException(arm::Excpt_SoftwareInterrupt);
    }

    core::cycles_t _THUMB_UndefinedInstruction(uint16_t instr) {
        return 1 + EnterException(arm::Excpt_UndefinedInstruction); // 1I + instruction fetch
    }

    core::cycles_t _THUMB_UnconditionalBranch(uint16_t instr) {
        int32_t offset = util::SignExtend<11, int32_t>(instr & 0x7FF) << 1;
        m_regs.r15 += offset;
        return ReloadPipelineTHUMB();
    }

    template <bool h>
    core::cycles_t _THUMB_LongBranchWithLink(uint16_t instr) {
        uint32_t offset = (instr & 0x7FF);
        auto &lr = m_regs.r14;
        auto &pc = m_regs.r15;
        if constexpr (h) {
            uint32_t nextAddr = pc - 2;
            pc = (lr + (offset << 1)) & ~1;
            lr = nextAddr | 1;
            return ReloadPipelineTHUMB();
        } else {
            lr = pc + util::SignExtend<23>(offset << 12);
            pc += 2;
            return AccessCyclesCSH(pc); // 1S to fetch next instruction
        }
    }

    core::cycles_t _THUMB_Unmapped(uint16_t instr) {
        throw std::runtime_error("Unmapped THUMB instruction");
    }

    // --- Lookup tables ------------------------------------------------------

    static constexpr std::array<bool, 256> s_conditionsTable = [] {
        std::array<bool, 256> arr{};
        for (uint8_t i = 0; i < 16; i++) {
            arm::ConditionFlags cond = static_cast<arm::ConditionFlags>(i);
            for (uint8_t flags = 0; flags < 0b10000; flags++) {
                bool &entry = arr[flags | (i << 4)];

                bool v = (flags >> 0) & 1;
                bool c = (flags >> 1) & 1;
                bool z = (flags >> 2) & 1;
                bool n = (flags >> 3) & 1;

                switch (cond) {
                case arm::Cond_EQ: entry = z; break;
                case arm::Cond_NE: entry = !z; break;
                case arm::Cond_CS: entry = c; break;
                case arm::Cond_CC: entry = !c; break;
                case arm::Cond_MI: entry = n; break;
                case arm::Cond_PL: entry = !n; break;
                case arm::Cond_VS: entry = v; break;
                case arm::Cond_VC: entry = !v; break;
                case arm::Cond_HI: entry = c && !z; break;
                case arm::Cond_LS: entry = !c || z; break;
                case arm::Cond_GE: entry = n == v; break;
                case arm::Cond_LT: entry = n != v; break;
                case arm::Cond_GT: entry = !z && (n == v); break;
                case arm::Cond_LE: entry = z || (n != v); break;
                case arm::Cond_AL: entry = true; break;
                case arm::Cond_NV: entry = false; break;
                }
            }
        }
        return arr;
    }();

    template <auto MemberFunc>
    [[gnu::flatten]] static core::cycles_t ARMInstrHandlerWrapper(ARM7TDMI &instance, uint32_t instr) {
        return (instance.*MemberFunc)(instr);
    }

    template <auto MemberFunc>
    [[gnu::flatten]] static core::cycles_t THUMBInstrHandlerWrapper(ARM7TDMI &instance, uint16_t instr) {
        return (instance.*MemberFunc)(instr);
    }

    template <uint32_t instr, bool debug>
    static constexpr auto MakeARMInstruction() {
        const auto op = (instr >> 9);
        if constexpr (op == 0b000) {
            if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0001) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_BranchAndExchange>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0011) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0110'0001) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0111) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1'1001'1111) == 0b1'0000'0101) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1'1001'1001) == 0b1'0000'1000) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1'1100'1111) == 0b0'0000'1001) {
                const bool a = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_MultiplyAccumulate<a, s>>;
            } else if constexpr ((instr & 0b1'1000'1111) == 0b0'1000'1001) {
                const bool u = (instr >> 6) & 1;
                const bool a = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_MultiplyAccumulateLong<u, a, s>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0000'1001) {
                const bool b = (instr >> 6) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_SingleDataSwap<b, debug>>;
            } else if constexpr ((instr & 0b1001) == 0b1001) {
                const bool p = (instr >> 8) & 1;
                const bool u = (instr >> 7) & 1;
                const bool i = (instr >> 6) & 1;
                const bool w = (instr >> 5) & 1;
                const bool l = (instr >> 4) & 1;
                const bool s = (instr >> 2) & 1;
                const bool h = (instr >> 1) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<
                    &ARM7TDMI::_ARM_HalfwordAndSignedDataTransfer<p, u, i, w, l, s, h, debug>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0000'0000) {
                const bool ps = (instr >> 6) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_MRS<ps>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0010'0000) {
                const bool pd = (instr >> 6) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_MSR<false, pd>>;
            } else {
                const uint8_t opcode = (instr >> 5) & 0xF;
                const bool s = (instr >> 4) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_DataProcessing<false, opcode, s>>;
            }
        } else if constexpr (op == 0b001) {
            if constexpr ((instr & 0b1'1011'0000) == 0b1'0010'0000) {
                const bool pd = (instr >> 6) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_MSR<true, pd>>;
            } else if constexpr ((instr & 0b1'1011'0000) == 0b1'0000'0000) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else {
                const uint8_t opcode = (instr >> 5) & 0xF;
                const bool s = (instr >> 4) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_DataProcessing<true, opcode, s>>;
            }
        } else if constexpr (op == 0b010 || op == 0b011) {
            const bool i = op & 1;
            if constexpr (i && (instr & 1)) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_UndefinedInstruction>;
            } else {
                const bool p = (instr >> 8) & 1;
                const bool u = (instr >> 7) & 1;
                const bool b = (instr >> 6) & 1;
                const bool w = (instr >> 5) & 1;
                const bool l = (instr >> 4) & 1;
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_SingleDataTransfer<i, p, u, b, w, l, debug>>;
            }
        } else if constexpr (op == 0b100) {
            const bool p = (instr >> 8) & 1;
            const bool u = (instr >> 7) & 1;
            const bool s = (instr >> 6) & 1;
            const bool w = (instr >> 5) & 1;
            const bool l = (instr >> 4) & 1;
            return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_BlockDataTransfer<p, u, s, w, l, debug>>;
        } else if constexpr (op == 0b101) {
            const bool l = (instr >> 8) & 1;
            return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_BranchAndBranchWithLink<l>>;
        } else if constexpr (op == 0b110) {
            // const bool p = (instr >> 8) & 1;
            // const bool u = (instr >> 7) & 1;
            // const bool n = (instr >> 6) & 1;
            // const bool w = (instr >> 5) & 1;
            // const bool l = (instr >> 4) & 1;
            // return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopDataTransfer<p, u, n, w, l>>;
            return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopDataTransfer>;
        } else if constexpr (op == 0b111) {
            const bool b24 = (instr >> 8) & 1;
            if constexpr (b24) {
                return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_SoftwareInterrupt>;
            } else {
                const bool b4 = (instr & 1);
                if constexpr (b4) {
                    // const uint8_t opcode1 = (instr >> 5) & 0x7;
                    // const bool s = (instr >> 4) & 1;
                    // return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopRegTransfer<opcode1, s>>;
                    const bool s = (instr >> 4) & 1;
                    return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopRegTransfer<s>>;
                } else {
                    // const uint8_t opcode1 = (instr >> 4) & 0xF;
                    // return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopDataOperations<opcode1>>;
                    return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_CopDataOperations>;
                }
            }
        } else {
            return &ARM7TDMI::ARMInstrHandlerWrapper<&ARM7TDMI::_ARM_Unmapped>;
        }
    }

    template <uint16_t instr, bool debug>
    static constexpr auto MakeTHUMBInstruction() {
        const auto op = (instr >> 6);
        if constexpr (op == 0b0000 || op == 0b0001) {
            const uint8_t op = (instr >> 5) & 0b11;
            if constexpr (op == 0b11) {
                const bool i = (instr >> 4) & 1;
                const bool op = (instr >> 3) & 1;
                const uint8_t rnOrOffset = (instr & 0b111);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_AddSub<i, op, rnOrOffset>>;
            } else {
                const uint8_t offset = (instr & 0b11111);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_MoveShiftedRegister<op, offset>>;
            }
        } else if constexpr (op == 0b0010 || op == 0b0011) {
            const uint8_t op = (instr >> 5) & 0b11;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_MovCmpAddSubImmediate<op, rd>>;
        } else if constexpr (op == 0b0100) {
            if constexpr (((instr >> 4) & 0b11) == 0b00) {
                const uint8_t op = (instr & 0b1111);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_ALUOperations<op>>;
            } else if constexpr (((instr >> 4) & 0b11) == 0b01) {
                const uint8_t op = (instr >> 2) & 0b11;
                const bool h1 = (instr >> 1) & 1;
                const bool h2 = (instr & 1);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_HiRegOperations<op, h1, h2>>;
            } else {
                const uint8_t rd = (instr >> 2) & 0b111;
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_PCRelativeLoad<rd, debug>>;
            }
        } else if constexpr (op == 0b0101) {
            if constexpr ((instr >> 3) & 1) {
                const bool h = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                const uint8_t ro = (instr & 0b111);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LoadStoreSignExtended<h, s, ro, debug>>;
            } else {
                const bool l = (instr >> 5) & 1;
                const bool b = (instr >> 4) & 1;
                const uint8_t ro = (instr & 0b111);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LoadStoreRegOffset<l, b, ro, debug>>;
            }
        } else if constexpr (op == 0b0110 || op == 0b0111) {
            const bool b = (instr >> 6) & 1;
            const bool l = (instr >> 5) & 1;
            const uint16_t offset = (instr & 0b11111) << (b ? 0 : 2);
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LoadStoreImmOffset<b, l, offset, debug>>;
        } else if constexpr (op == 0b1000) {
            const bool l = (instr >> 5) & 1;
            const uint16_t offset = (instr & 0b11111) << 1;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LoadStoreHalfWord<l, offset, debug>>;
        } else if constexpr (op == 0b1001) {
            const bool l = (instr >> 5) & 1;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_SPRelativeLoadStore<l, rd, debug>>;
        } else if constexpr (op == 0b1010) {
            const bool sp = (instr >> 5) & 1;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LoadAddress<sp, rd>>;
        } else if constexpr (op == 0b1011) {
            if constexpr (((instr >> 2) & 0b1111) == 0b0000) {
                const bool s = (instr >> 1) & 1;
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_AddOffsetToSP<s>>;
            } else if constexpr (((instr >> 2) & 0b1111) == 0b1110) {
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_UndefinedInstruction>;
            } else if constexpr (((instr >> 2) & 0b0110) == 0b0100) {
                const bool l = (instr >> 5) & 1;
                const bool r = (instr >> 2) & 1;
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_PushPopRegs<l, r, debug>>;
            } else {
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_UndefinedInstruction>;
            }
        } else if constexpr (op == 0b1100) {
            const bool l = (instr >> 5) & 1;
            const uint8_t rb = (instr >> 2) & 0b111;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_MultipleLoadStore<l, rb, debug>>;
        } else if constexpr (op == 0b1101) {
            if constexpr (((instr >> 2) & 0b1111) == 0b1111) {
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_SoftwareInterrupt>;
            } else if constexpr (((instr >> 2) & 0b1111) == 0b1110) {
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_UndefinedInstruction>;
            } else {
                const arm::ConditionFlags cond = static_cast<arm::ConditionFlags>((instr >> 2) & 0xF);
                return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_ConditionalBranch<cond>>;
            }
        } else if constexpr (op == 0b1110) {
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_UnconditionalBranch>;
        } else if constexpr (op == 0b1111) {
            const bool h = (instr >> 5) & 1;
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_LongBranchWithLink<h>>;
        } else {
            return &ARM7TDMI::THUMBInstrHandlerWrapper<&ARM7TDMI::_THUMB_Unmapped>;
        }
    }

    template <bool debug>
    static constexpr auto MakeARMTable() {
        std::array<ARMInstructionHandler<MI, Exec>, 4096> table{};
        util::constexpr_for<256>([&](auto i) {
            table[i * 16 + 0] = MakeARMInstruction<i * 16 + 0, debug>();
            table[i * 16 + 1] = MakeARMInstruction<i * 16 + 1, debug>();
            table[i * 16 + 2] = MakeARMInstruction<i * 16 + 2, debug>();
            table[i * 16 + 3] = MakeARMInstruction<i * 16 + 3, debug>();
            table[i * 16 + 4] = MakeARMInstruction<i * 16 + 4, debug>();
            table[i * 16 + 5] = MakeARMInstruction<i * 16 + 5, debug>();
            table[i * 16 + 6] = MakeARMInstruction<i * 16 + 6, debug>();
            table[i * 16 + 7] = MakeARMInstruction<i * 16 + 7, debug>();
            table[i * 16 + 8] = MakeARMInstruction<i * 16 + 8, debug>();
            table[i * 16 + 9] = MakeARMInstruction<i * 16 + 9, debug>();
            table[i * 16 + 10] = MakeARMInstruction<i * 16 + 10, debug>();
            table[i * 16 + 11] = MakeARMInstruction<i * 16 + 11, debug>();
            table[i * 16 + 12] = MakeARMInstruction<i * 16 + 12, debug>();
            table[i * 16 + 13] = MakeARMInstruction<i * 16 + 13, debug>();
            table[i * 16 + 14] = MakeARMInstruction<i * 16 + 14, debug>();
            table[i * 16 + 15] = MakeARMInstruction<i * 16 + 15, debug>();
        });
        return table;
    }

    template <bool debug>
    static constexpr auto MakeTHUMBTable() {
        std::array<THUMBInstructionHandler<MI, Exec>, 1024> table{};
        util::constexpr_for<256>([&](auto i) {
            table[i * 4 + 0] = MakeTHUMBInstruction<i * 4 + 0, debug>();
            table[i * 4 + 1] = MakeTHUMBInstruction<i * 4 + 1, debug>();
            table[i * 4 + 2] = MakeTHUMBInstruction<i * 4 + 2, debug>();
            table[i * 4 + 3] = MakeTHUMBInstruction<i * 4 + 3, debug>();
        });
        return table;
    }

    static constexpr auto s_armTable = MakeARMTable<false>();
    static constexpr auto s_thumbTable = MakeTHUMBTable<false>();

    static constexpr auto s_armDebugTable = MakeARMTable<true>();
    static constexpr auto s_thumbDebugTable = MakeTHUMBTable<true>();

    friend class UncachedExecutor<MI>;
    friend class CachedExecutor<MI>;
};

// --- UncachedExecutor implementation -------------------------------------------------------------

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::Reset() {
    m_pipeline[0] = m_pipeline[1] = 0xF0000000; // "Never" condition, instruction doesn't matter
}

template <typename MI>
ALWAYS_INLINE core::cycles_t UncachedExecutor<MI>::Run(bool enableExecHooks, bool debug, bool /*singleStep*/) {
    auto &regs = m_arm.m_regs;
    auto instr = m_pipeline[0];
    if (regs.cpsr.t) {
        assert((regs.r15 & 1) == 0);
        assert((instr & 0xFFFF0000) == 0);
        if (enableExecHooks) {
            if (auto hook = execHooks[regs.r15 - 4]) [[unlikely]] {
                hook.fn(hook.context, regs.r15 - 4, instr, hooks::CPU::ARM7, hooks::InstrType::Thumb);
            }
        }
        if (debug) {
            if (m_arm.CheckInstructionBreakpoint(regs.r15 - 4, instr)) {
                return 0;
            }
        }
        m_pipeline[0] = m_pipeline[1];
        m_pipeline[1] = m_arm.CodeReadHalf(regs.r15);
        const auto &table = debug ? m_arm.s_thumbDebugTable : m_arm.s_thumbTable;
        return table[instr >> 6](m_arm, instr);
    } else {
        assert((regs.r15 & 3) == 0);
        if (enableExecHooks) {
            if (auto hook = execHooks[regs.r15 - 8]) [[unlikely]] {
                hook.fn(hook.context, regs.r15 - 8, instr, hooks::CPU::ARM7, hooks::InstrType::ARM);
            }
        }
        if (debug) {
            if (m_arm.CheckInstructionBreakpoint(regs.r15 - 8, instr)) {
                return 0;
            }
        }
        m_pipeline[0] = m_pipeline[1];
        m_pipeline[1] = m_arm.CodeReadWord(regs.r15);
        if (m_arm.EvalCondition(static_cast<arm::ConditionFlags>(instr >> 28))) {
            const size_t index = ((instr >> 16) & 0b1111'1111'0000) | ((instr >> 4) & 0b1111);
            const auto &table = debug ? m_arm.s_armDebugTable : m_arm.s_armTable;
            return table[index](m_arm, instr);
        } else {
            regs.r15 += 4;
            return m_arm.AccessCyclesCSW(regs.r15);
        }
    }
}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::FillPipeline() {
    if (m_arm.m_regs.cpsr.t) {
        m_pipeline[0] = m_arm.CodeReadHalf(m_arm.m_regs.r15 + 0);
        m_pipeline[1] = m_arm.CodeReadHalf(m_arm.m_regs.r15 + 2);
    } else {
        m_pipeline[0] = m_arm.CodeReadWord(m_arm.m_regs.r15 + 0);
        m_pipeline[1] = m_arm.CodeReadWord(m_arm.m_regs.r15 + 4);
    }
}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::ReloadPipelineARM() {
    m_pipeline[0] = m_arm.CodeReadWord(m_arm.m_regs.r15 + 0);
    m_pipeline[1] = m_arm.CodeReadWord(m_arm.m_regs.r15 + 4);
}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::ReloadPipelineTHUMB() {
    m_pipeline[0] = m_arm.CodeReadHalf(m_arm.m_regs.r15 + 0);
    m_pipeline[1] = m_arm.CodeReadHalf(m_arm.m_regs.r15 + 2);
}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::Stall() {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::HitBreakpoint() {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::ChangeExecState(arm::ExecState execState) {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::ClearCache() {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::InvalidateCache() {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::InvalidateCacheAddress(uint32_t address) {}

template <typename MI>
ALWAYS_INLINE void UncachedExecutor<MI>::InvalidateCacheRange(uint32_t start, uint32_t end) {}

template <typename MI>
ALWAYS_INLINE uint32_t UncachedExecutor<MI>::GetPipelineFetchSlotOpcode() {
    return m_pipeline[1];
}

template <typename MI>
uint32_t UncachedExecutor<MI>::GetPipelineDecodeSlotOpcode() {
    return m_pipeline[0];
}

// --- CachedExecutor implementation ---------------------------------------------------------------

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::Reset() {
    for (size_t i = 0; i < kNumPages; i++) {
        if (m_armCache[i]) {
            m_armCache[i].reset();
        }
        if (m_thumbCache[i]) {
            m_thumbCache[i].reset();
        }
    }
}

template <typename MI>
ALWAYS_INLINE core::cycles_t CachedExecutor<MI>::Run(bool enableExecHooks, bool debug, bool singleStep) {
    auto &arm = m_arm;
    auto &regs = arm.m_regs;
    core::cycles_t cycles = 0;
    if (regs.cpsr.t) {
        const uint32_t pc = regs.r15 - 4;
        uint32_t index = (pc >> 1) & kTHUMBAddressMask;
        ThumbBlock &block = GetCachedThumbCode(pc);
        m_cacheValid = true;
        do {
            auto &instr = block.instrs[index];
            if (enableExecHooks) {
                auto pc = regs.r15 - 4;
                pc &= ~kAddressMask;
                pc += index << 1;
                if (auto hook = execHooks[pc]) [[unlikely]] {
                    hook.fn(hook.context, pc, instr.instr, hooks::CPU::ARM7, hooks::InstrType::Thumb);
                }
            }
            if (debug) {
                auto pc = regs.r15 - 4;
                pc &= ~kAddressMask;
                pc += index << 1;
                if (arm.CheckInstructionBreakpoint(pc, instr.instr)) {
                    break;
                }
            }
            if (debug) {
                cycles += instr.debugHandler(arm, instr.instr);
            } else {
                cycles += instr.handler(arm, instr.instr);
            }
            if (++index == kTHUMBEntries) {
                break;
            }
            if (!m_cacheValid) {
                break;
            }
        } while (!singleStep);
    } else {
        const uint32_t pc = regs.r15 - 8;
        uint32_t index = (pc >> 2) & kARMAddressMask;
        ARMBlock &block = GetCachedARMCode(pc);
        m_cacheValid = true;
        do {
            auto &instr = block.instrs[index];
            if (enableExecHooks) {
                auto pc = regs.r15 - 8;
                pc &= ~kAddressMask;
                pc += index << 2;
                if (auto hook = execHooks[pc]) [[unlikely]] {
                    hook.fn(hook.context, pc, instr.instr, hooks::CPU::ARM7, hooks::InstrType::ARM);
                }
            }
            if (debug) {
                auto pc = regs.r15 - 8;
                pc &= ~kAddressMask;
                pc += index << 2;
                if (arm.CheckInstructionBreakpoint(pc, instr.instr)) {
                    break;
                }
            }
            if (arm.EvalCondition(static_cast<arm::ConditionFlags>(instr.instr >> 28))) {
                if (debug) {
                    cycles += instr.debugHandler(arm, instr.instr);
                } else {
                    cycles += instr.handler(arm, instr.instr);
                }
            } else {
                // TODO: consider precomputing R15?
                regs.r15 += 4;
                cycles += arm.AccessCyclesCSW(regs.r15);
            }
            if (++index == kARMEntries) {
                break;
            }
            if (!m_cacheValid) {
                break;
            }
        } while (!singleStep);
    }
    return cycles;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::FillPipeline() {}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::ReloadPipelineARM() {
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::ReloadPipelineTHUMB() {
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::Stall() {
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::HitBreakpoint() {
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::ChangeExecState(arm::ExecState execState) {
    if (execState != arm::ExecState::Run) {
        m_cacheValid = false;
    }
}

template <typename MI>
ALWAYS_INLINE uint32_t CachedExecutor<MI>::TranslateAddress(uint32_t address) {
    if constexpr (config::translateAddressesInCachedExecutor) {
        return m_arm.m_mem.TranslateAddress(address);
    } else {
        return address;
    }
}

template <typename MI>
ALWAYS_INLINE typename CachedExecutor<MI>::ARMBlock &CachedExecutor<MI>::GetCachedARMCode(uint32_t address) {
    const uint32_t translatedAddress = TranslateAddress(address);
    const uint32_t page = (translatedAddress >> kPageShift);
    const uint32_t entry = (translatedAddress >> kBlockBits) & kEntryMask;
    const uint32_t baseAddress = (translatedAddress & ~kAddressMask);

    // Allocate page if needed
    if (!m_armCache[page]) [[unlikely]] {
        m_armCache[page] = std::make_unique<CachePage<ARMBlock>>();
    }
    auto &cachePage = *m_armCache[page];

    // Reset page if invalid
    if (!cachePage.pageValid) [[unlikely]] {
        cachePage.valid.reset();
        cachePage.pageValid = true;
    }

    // Fetch and decode instructions if the entry is invalid
    auto &block = cachePage[entry];
    if (!cachePage.valid.test(entry)) [[unlikely]] {
        for (size_t i = 0; i < kARMEntries; i++) {
            const uint32_t instr = m_arm.CodeReadWord(baseAddress + i * 4);
            const size_t index = ((instr >> 16) & 0b1111'1111'0000) | ((instr >> 4) & 0b1111);

            block.instrs[i].instr = instr;
            block.instrs[i].handler = m_arm.s_armTable[index];
            block.instrs[i].debugHandler = m_arm.s_armDebugTable[index];
        }
        cachePage.valid.set(entry);
    }
    return block;
}

template <typename MI>
ALWAYS_INLINE typename CachedExecutor<MI>::ThumbBlock &CachedExecutor<MI>::GetCachedThumbCode(uint32_t address) {
    const uint32_t translatedAddress = TranslateAddress(address);
    const uint32_t page = (translatedAddress >> kPageShift);
    const uint32_t entry = (translatedAddress >> kBlockBits) & kEntryMask;
    const uint32_t baseAddress = (translatedAddress & ~kAddressMask);

    // Allocate page if needed
    if (!m_thumbCache[page]) [[unlikely]] {
        m_thumbCache[page] = std::make_unique<CachePage<ThumbBlock>>();
    }
    auto &cachePage = *m_thumbCache[page];

    // Reset page if invalid
    if (!cachePage.pageValid) [[unlikely]] {
        cachePage.valid.reset();
        cachePage.pageValid = true;
    }

    // Fetch and decode instructions if the entry is invalid
    auto &block = cachePage[entry];
    if (!cachePage.valid.test(entry)) [[unlikely]] {
        for (size_t i = 0; i < kTHUMBEntries; i++) {
            const uint16_t instr = m_arm.CodeReadHalf(baseAddress + i * 2);

            block.instrs[i].instr = instr;
            block.instrs[i].handler = m_arm.s_thumbTable[instr >> 6];
            block.instrs[i].debugHandler = m_arm.s_thumbDebugTable[instr >> 6];
        }
        cachePage.valid.set(entry);
    }
    return block;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::ClearCache() {
    for (size_t i = 0; i < kNumPages; i++) {
        if (m_armCache[i]) {
            m_armCache[i].reset();
        }
        if (m_thumbCache[i]) {
            m_thumbCache[i].reset();
        }
    }
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::InvalidateCache() {
    for (size_t i = 0; i < kNumPages; i++) {
        if (m_armCache[i]) {
            m_armCache[i]->pageValid = false;
        }
        if (m_thumbCache[i]) {
            m_thumbCache[i]->pageValid = false;
        }
    }
    m_cacheValid = false;
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::InvalidateCacheAddress(uint32_t address) {
    const uint32_t translatedAddress = TranslateAddress(address);
    const uint32_t page = (translatedAddress >> kPageShift);
    const uint32_t entry = (translatedAddress >> kBlockBits) & kEntryMask;
    if (m_armCache[page]) {
        m_armCache[page]->valid.set(entry, false);
    }
    if (m_thumbCache[page]) {
        m_thumbCache[page]->valid.set(entry, false);
    }

    uint32_t pc = m_arm.m_regs.r15;
    pc -= (m_arm.m_regs.cpsr.t ? 4 : 8);
    pc &= ~kAddressMask;
    if (pc == (address & ~kAddressMask)) {
        m_cacheValid = false;
    }
}

template <typename MI>
ALWAYS_INLINE void CachedExecutor<MI>::InvalidateCacheRange(uint32_t start, uint32_t end) {
    if (start > end) {
        std::swap(start, end);
    }
    for (uint32_t address = start; address < end; address += kBlockSize) {
        const uint32_t translatedAddress = TranslateAddress(address);
        const uint32_t page = (translatedAddress >> kPageShift);
        const uint32_t entry = (translatedAddress >> kBlockBits) & kEntryMask;
        if (m_armCache[page]) {
            m_armCache[page]->valid.set(entry, false);
        }
        if (m_thumbCache[page]) {
            m_thumbCache[page]->valid.set(entry, false);
        }
    }

    uint32_t pc = m_arm.m_regs.r15;
    pc -= (m_arm.m_regs.cpsr.t ? 4 : 8);
    pc &= ~kAddressMask;
    if (pc >= (start & ~kAddressMask) && pc <= ((end + kAddressMask - 1) & ~kAddressMask)) {
        m_cacheValid = false;
    }
}

template <typename MI>
ALWAYS_INLINE uint32_t CachedExecutor<MI>::GetPipelineFetchSlotOpcode() {
    if (m_arm.m_regs.cpsr.t) {
        return m_arm.CodeReadHalf(m_arm.m_regs.r15 - 4);
    } else {
        return m_arm.CodeReadWord(m_arm.m_regs.r15 - 8);
    }
}

template <typename MI>
uint32_t CachedExecutor<MI>::GetPipelineDecodeSlotOpcode() {
    if (m_arm.m_regs.cpsr.t) {
        return m_arm.CodeReadHalf(m_arm.m_regs.r15 - 2);
    } else {
        return m_arm.CodeReadWord(m_arm.m_regs.r15 - 4);
    }
}

} // namespace advanDS::cpu::interp::arm7tdmi
