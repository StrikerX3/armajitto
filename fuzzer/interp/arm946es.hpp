#pragma once

#include <armajitto/core/system_interface.hpp>

#include "arm/arithmetic.hpp"
#include "arm/conditions.hpp"
#include "arm/exceptions.hpp"
#include "arm/registers.hpp"

#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace bit {

// Sign-extend from a constant bit width
template <unsigned B, std::integral T>
constexpr auto sign_extend(const T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

} // namespace bit

namespace util {

namespace detail {

    template <typename F, std::size_t... S>
    constexpr void constexpr_for_impl(F &&function, std::index_sequence<S...>) {
        (function(std::integral_constant<std::size_t, S>{}), ...);
    }

} // namespace detail

template <std::size_t iterations, typename F>
constexpr void constexpr_for(F &&function) {
    detail::constexpr_for_impl(std::forward<F>(function), std::make_index_sequence<iterations>());
}

template <typename T>
inline T MemRead(const uint8_t *mem, uint32_t address) {
    return *reinterpret_cast<const T *>(&mem[address]);
}

template <typename T>
inline void MemWrite(uint8_t *mem, uint32_t address, T value) {
    *reinterpret_cast<T *>(&mem[address]) = value;
}

} // namespace util

namespace core {

using cycles_t = uint64_t;

} // namespace core

namespace interp::arm946es {

struct CP15 {
    union CP15ControlRegister {
        uint32_t u32;
        struct {
            uint32_t             //
                puEnable : 1,    // 0  MMU/PU Enable         (0=Disable, 1=Enable) (Fixed 0 if none)
                a : 1,           // 1  Alignment Fault Check (0=Disable, 1=Enable) (Fixed 0/1 if none/always on)
                dataCache : 1,   // 2  Data/Unified Cache    (0=Disable, 1=Enable) (Fixed 0/1 if none/always on)
                writeBuffer : 1, // 3  Write Buffer          (0=Disable, 1=Enable) (Fixed 0/1 if none/always on)
                p : 1,           // 4  Exception Handling    (0=26bit, 1=32bit)    (Fixed 1 if always 32bit)
                d : 1,           // 5  26bit-address faults  (0=Enable, 1=Disable) (Fixed 1 if always 32bit)
                l : 1,           // 6  Abort Model (pre v4)  (0=Early, 1=Late Abort) (Fixed 1 if ARMv4 and up)
                bigEndian : 1,   // 7  Endian                (0=Little, 1=Big)     (Fixed 0/1 if fixed)
                s : 1,           // 8  System Protection bit (MMU-only)
                r : 1,           // 9  ROM Protection bit    (MMU-only)
                f : 1,           // 10 Implementation defined
                z : 1,           // 11 Branch Prediction     (0=Disable, 1=Enable)
                codeCache : 1,   // 12 Instruction Cache     (0=Disable, 1=Enable) (ignored if Unified cache)
                v : 1,           // 13 Exception Vectors     (0=00000000h, 1=FFFF0000h)
                rr : 1,          // 14 Cache Replacement     (0=Normal/PseudoRandom, 1=Predictable/RoundRobin)
                preARMv5 : 1,    // 15 Pre-ARMv5 Mode        (0=Normal, 1=Pre ARMv5, LDM/LDR/POP_PC.Bit0/Thumb)
                dtcmEnable : 1,  // 16 DTCM Enable           (0=Disable, 1=Enable)
                dtcmLoad : 1,    // 17 DTCM Load Mode        (0=R/W, 1=DTCM Write-only)
                itcmEnable : 1,  // 18 ITCM Enable           (0=Disable, 1=Enable)
                itcmLoad : 1,    // 19 ITCM Load Mode        (0=R/W, 1=ITCM Write-only)
                : 1,             // 20 Reserved              (0)
                : 1,             // 21 Reserved              (0)
                : 1,             // 22 Unaligned Access      (?=Enable unaligned access and mixed endian)
                : 1,             // 23 Extended Page Table   (0=Subpage AP Bits Enabled, 1=Disabled)
                : 1,             // 24 Reserved              (0)
                : 1,             // 25 CPSR E on exceptions  (0=Clear E bit, 1=Set E bit)
                : 1,             // 26 Reserved              (0)
                : 1,             // 27 FIQ Behaviour         (0=Normal FIQ behaviour, 1=FIQs behave as NMFI)
                : 1,             // 28 TEX Remap bit         (0=No remapping, 1=Remap registers used)
                : 1,             // 29 Force AP              (0=Access Bit not used, 1=AP[0] used as Access bit)
                : 1,             // 30 Reserved              (0)
                : 1;             // 31 Reserved              (0)
        };
    } ctl;

    struct ProtectionUnit {
        uint32_t dataCachabilityBits;
        uint32_t codeCachabilityBits;
        uint32_t bufferabilityBits;

        uint32_t dataAccessPermissions;
        uint32_t codeAccessPermissions;

        union Region {
            uint32_t u32;
            struct {
                uint32_t           //
                    enable : 1,    // 0     Protection Region Enable (0=Disable, 1=Enable)
                    size : 5,      // 1-5   Protection Region Size   (2 SHL X) ;min=(X=11)=4KB, max=(X=31)=4GB
                    : 6,           // 6-11  Reserved/zero
                    baseAddr : 20; // 12-31 Protection Region Base address (Addr = Y*4K; must be SIZE-aligned)
            };
        };

        Region regions[8];
    } pu;

    uint32_t dtcmParams;
    uint32_t itcmParams;
};

// --- ARM core ------------------------------------------------------------------------------------

// ARM946E-S CPU emulator
template <typename Sys>
class ARM946ES final {
    using ARMInstructionHandler = core::cycles_t (*)(ARM946ES &, uint32_t);
    using THUMBInstructionHandler = core::cycles_t (*)(ARM946ES &, uint16_t);

    static_assert(std::is_base_of_v<armajitto::ISystem, Sys>, "System must implement armajitto::ISystem");

public:
    ARM946ES(Sys &sys)
        : m_sys(sys) {

        Reset();
    }

    void Reset() {
        // Reset CP15
        m_cp15.ctl.u32 = 0x2078;

        std::fill(std::begin(m_itcm), std::end(m_itcm), 0);
        std::fill(std::begin(m_dtcm), std::end(m_dtcm), 0);

        m_itcmWriteSize = m_itcmReadSize = 0;
        m_cp15.itcmParams = 0;

        m_dtcmBase = 0xFFFFFFFF;
        m_dtcmWriteSize = m_dtcmReadSize = 0;
        m_cp15.dtcmParams = 0;

        m_baseVectorAddress = 0xFFFF0000;

        m_cp15.pu.dataCachabilityBits = 0;
        m_cp15.pu.codeCachabilityBits = 0;
        m_cp15.pu.bufferabilityBits = 0;
        m_cp15.pu.dataAccessPermissions = 0;
        m_cp15.pu.codeAccessPermissions = 0;
        for (size_t i = 0; i < 8; i++) {
            m_cp15.pu.regions[i].u32 = 0;
        }

        // Reset CPU
        m_regs.Reset();
        SetMode(arm::Mode::Supervisor);
        m_regs.cpsr.i = 1;
        m_regs.cpsr.f = 1;
        m_regs.cpsr.t = 0;
        m_spsr = &m_regs.cpsr;
        m_regs.r13 = 0x3007F00;
        m_regs.r15 = m_baseVectorAddress;
        m_execState = arm::ExecState::Run;

        m_pipeline[0] = m_pipeline[1] = 0xE1A00000; // MOV r0, r0  (aka NOP)
    }

    void FillPipeline() {
        if (m_regs.cpsr.t) {
            m_pipeline[0] = CodeReadHalf(m_regs.r15 + 0);
            m_pipeline[1] = CodeReadHalf(m_regs.r15 + 2);
        } else {
            m_pipeline[0] = CodeReadWord(m_regs.r15 + 0);
            m_pipeline[1] = CodeReadWord(m_regs.r15 + 4);
        }
        m_regs.r15 += (m_regs.cpsr.t ? 4 : 8);
    }

    // Executes one instruction or block
    core::cycles_t Run() {
        auto instr = m_pipeline[0];
        if (m_regs.cpsr.t) {
            assert((m_regs.r15 & 1) == 0);
            assert((instr & 0xFFFF0000) == 0);
            m_pipeline[0] = m_pipeline[1];
            m_pipeline[1] = CodeReadHalf(m_regs.r15);
            const auto &table = s_thumbTable;
            return table[instr >> 6](*this, instr);
        } else {
            assert((m_regs.r15 & 3) == 0);
            m_pipeline[0] = m_pipeline[1];
            m_pipeline[1] = CodeReadWord(m_regs.r15);
            if (EvalCondition(static_cast<arm::ConditionFlags>(instr >> 28))) {
                const size_t index = ((instr >> 16) & 0b1111'1111'0000) | ((instr >> 4) & 0b1111);
                const size_t condIndex = ((instr >> 28) + 1) >> 4;
                const auto &table = s_armTable;
                return table[condIndex][index](*this, instr);
            } else {
                m_regs.r15 += 4;
                return 1;
            }
        }
    }

    // Enters the IRQ exception vector
    core::cycles_t HandleIRQ() {
        if (m_regs.cpsr.i) {
            return 0;
        }
        return EnterException(arm::Excpt_NormalInterrupt);
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

    bool SetSPSR(arm::PSR psr) {
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
    }

    bool HasCoprocessor(uint8_t cop) const {
        return cop == 15;
    }

    uint32_t CPRead(uint8_t cop, uint16_t reg) const {
        switch (cop) {
        case 15: return CP15Read(reg);
        default: return 0;
        }
    }

    void CPWrite(uint8_t cop, uint16_t reg, uint32_t value) {
        switch (cop) {
        case 15: CP15Write(reg, value); break;
        default: break;
        }
    }

    const CP15 &GetCP15() const {
        return m_cp15;
    }

    uint32_t CP15Read(uint16_t reg) const {
        switch (reg) {
        case 0x000: // C0,C0,0 - Main ID Register
        case 0x003: // C0,C0,3 - Reserved - copy of C0,C0,0
        case 0x004: // C0,C0,4 - Reserved - copy of C0,C0,0
        case 0x005: // C0,C0,5 - Reserved - copy of C0,C0,0
        case 0x006: // C0,C0,6 - Reserved - copy of C0,C0,0
        case 0x007: // C0,C0,7 - Reserved - copy of C0,C0,0
            // ARMv5TE, ARM946, rev1, variant 0, manufactured by ARM
            return 0x41059461;
        case 0x001: // C0,C0,1 - Cache Type Register
            // Code=2000h bytes, Data=1000h bytes, assoc=whatever, and line size 32 bytes each
            return 0x0F0D2112;
        case 0x002: // C0,C0,2 - Tightly Coupled Memory (TCM) Size Register
            // ITCM present, size = 32 KiB; DTCM present, size = 16 KiB
            return 0x00140180;

        case 0x100: // C1,C0,0 - Control Register
            return m_cp15.ctl.u32;

        case 0x200: // C2,C0,0 - Cachability Bits for Data/Unified Protection Region
            return m_cp15.pu.dataCachabilityBits;
        case 0x201: // C2,C0,1 - Cachability Bits for Instruction Protection Region
            return m_cp15.pu.codeCachabilityBits;
        case 0x300: // C3,C0,0 - Cache Write-Bufferability Bits for Data Protection Regions
            return m_cp15.pu.bufferabilityBits;

        case 0x500: { // C5,C0,0 - Access Permission Data/Unified Protection Region
            uint32_t value = 0;
            for (size_t i = 0; i < 8; i++) {
                value |= (m_cp15.pu.dataAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
            }
            return value;
        }
        case 0x501: { // C5,C0,1 - Access Permission Instruction Protection Region
            uint32_t value = 0;
            for (size_t i = 0; i < 8; i++) {
                value |= (m_cp15.pu.codeAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
            }
            return value;
        }
        case 0x502: // C5,C0,2 - Extended Access Permission Data/Unified Protection Region
            return m_cp15.pu.dataAccessPermissions;
        case 0x503: // C5,C0,3 - Extended Access Permission Instruction Protection Region
            return m_cp15.pu.codeAccessPermissions;

        case 0x600: // C6,C0,0 - Protection Unit Data/Unified Region 0
        case 0x601: // C6,C0,1 - Protection Unit Instruction Region 0
        case 0x610: // C6,C1,0 - Protection Unit Data/Unified Region 1
        case 0x611: // C6,C1,1 - Protection Unit Instruction Region 1
        case 0x620: // C6,C2,0 - Protection Unit Data/Unified Region 2
        case 0x621: // C6,C2,1 - Protection Unit Instruction Region 2
        case 0x630: // C6,C3,0 - Protection Unit Data/Unified Region 3
        case 0x631: // C6,C3,1 - Protection Unit Instruction Region 3
        case 0x640: // C6,C4,0 - Protection Unit Data/Unified Region 4
        case 0x641: // C6,C4,1 - Protection Unit Instruction Region 4
        case 0x650: // C6,C5,0 - Protection Unit Data/Unified Region 5
        case 0x651: // C6,C5,1 - Protection Unit Instruction Region 5
        case 0x660: // C6,C6,0 - Protection Unit Data/Unified Region 6
        case 0x661: // C6,C6,1 - Protection Unit Instruction Region 6
        case 0x670: // C6,C7,0 - Protection Unit Data/Unified Region 7
        case 0x671: // C6,C7,1 - Protection Unit Instruction Region 7
            return m_cp15.pu.regions[(reg >> 4) & 0xF].u32;

        case 0x910: // C9,C1,0 - Data TCM Size/Base
            return m_cp15.dtcmParams;
        case 0x911: // C9,C1,1 - Instruction TCM Size/Base
            return m_cp15.itcmParams;

        default: return 0;
        }
    }

    void CP15Write(uint16_t reg, uint32_t value) {
        switch (reg) {
        case 0x100: { // C1,C0,0 - Control Register
            m_cp15.ctl.u32 = (m_cp15.ctl.u32 & ~0x000FF085) | (value & 0x000FF085);
            // TODO: check for big-endian mode, support it if needed
            m_baseVectorAddress = (m_cp15.ctl.v) ? 0xFFFF0000 : 0x00000000;
            ConfigureDTCM();
            ConfigureITCM();
            break;
        }

        case 0x200: { // C2,C0,0 - Cachability Bits for Data/Unified Protection Region
            m_cp15.pu.dataCachabilityBits = value;
            break;
        }
        case 0x201: { // C2,C0,1 - Cachability Bits for Instruction Protection Region
            m_cp15.pu.codeCachabilityBits = value;
            break;
        }
        case 0x300: { // C3,C0,0 - Cache Write-Bufferability Bits for Data Protection Regions
            m_cp15.pu.bufferabilityBits = value;
            break;
        }

        case 0x500: { // C5,C0,0 - Access Permission Data/Unified Protection Region
            auto &bits = m_cp15.pu.dataAccessPermissions;
            bits = 0;
            for (size_t i = 0; i < 8; i++) {
                bits |= (value & (0x3 << (i * 2))) << (i * 2);
            }
            break;
        }
        case 0x501: { // C5,C0,1 - Access Permission Instruction Protection Region
            auto &bits = m_cp15.pu.codeAccessPermissions;
            bits = 0;
            for (size_t i = 0; i < 8; i++) {
                bits |= (value & (0x3 << (i * 2))) << (i * 2);
            }
            break;
        }
        case 0x502: { // C5,C0,2 - Extended Access Permission Data/Unified Protection Region
            m_cp15.pu.dataAccessPermissions = value;
            break;
        }
        case 0x503: { // C5,C0,3 - Extended Access Permission Instruction Protection Region
            m_cp15.pu.codeAccessPermissions = value;
            break;
        }

        case 0x600: // C6,C0,0 - Protection Unit Data/Unified Region 0
        case 0x601: // C6,C0,1 - Protection Unit Instruction Region 0
        case 0x610: // C6,C1,0 - Protection Unit Data/Unified Region 1
        case 0x611: // C6,C1,1 - Protection Unit Instruction Region 1
        case 0x620: // C6,C2,0 - Protection Unit Data/Unified Region 2
        case 0x621: // C6,C2,1 - Protection Unit Instruction Region 2
        case 0x630: // C6,C3,0 - Protection Unit Data/Unified Region 3
        case 0x631: // C6,C3,1 - Protection Unit Instruction Region 3
        case 0x640: // C6,C4,0 - Protection Unit Data/Unified Region 4
        case 0x641: // C6,C4,1 - Protection Unit Instruction Region 4
        case 0x650: // C6,C5,0 - Protection Unit Data/Unified Region 5
        case 0x651: // C6,C5,1 - Protection Unit Instruction Region 5
        case 0x660: // C6,C6,0 - Protection Unit Data/Unified Region 6
        case 0x661: // C6,C6,1 - Protection Unit Instruction Region 6
        case 0x670: // C6,C7,0 - Protection Unit Data/Unified Region 7
        case 0x671: // C6,C7,1 - Protection Unit Instruction Region 7
            m_cp15.pu.regions[(reg >> 4) & 0xF].u32 = value;
            break;

        case 0x704: // C7,C0,4 - Wait For Interrupt (Halt)
        case 0x782: // C7,C8,2 - Wait For Interrupt (Halt), alternately to C7,C0,4
            m_execState = arm::ExecState::Halt;
            break;

        case 0x750: // C7,C5,0 - Invalidate Entire Instruction Cache
            break;
        case 0x751: { // C7,C5,1 - Invalidate Instruction Cache Line
            break;
        }
        case 0x752: // C7,C5,2 - Invalidate Instruction Cache Line
            // TODO: implement
            break;

        case 0x760: // C7,C6,0 - Invalidate Entire Data Cache
            // TODO: implement
            break;
        case 0x761: // C7,C6,1 - Invalidate Data Cache Line
            // TODO: implement
            break;
        case 0x762: // C7,C6,2 - Invalidate Data Cache Line
            // TODO: implement
            break;

        case 0x7A1: // C7,C10,1 - Clean Data Cache Line
            // TODO: implement
            break;
        case 0x7A2: // C7,C10,2 - Clean Data Cache Line
            // TODO: implement
            break;

        case 0x910: // C9,C1,0 - Data TCM Size/Base
            m_cp15.dtcmParams = value;
            ConfigureDTCM();
            break;
        case 0x911: // C9,C1,1 - Instruction TCM Size/Base
            m_cp15.itcmParams = value;
            ConfigureITCM();
            break;
        }
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

private:
    arm::Registers m_regs;
    Sys &m_sys;
    arm::PSR *m_spsr;
    arm::ExecState m_execState;
    uint32_t m_baseVectorAddress = 0xFFFF0000;
    uint32_t m_pipeline[2];

    // --- CP15 ---------------------------------------------------------------

    alignas(4096) uint8_t m_itcm[0x8000];
    alignas(4096) uint8_t m_dtcm[0x4000];

    uint32_t m_itcmWriteSize;
    uint32_t m_itcmReadSize;

    uint32_t m_dtcmBase;
    uint32_t m_dtcmWriteSize;
    uint32_t m_dtcmReadSize;

    CP15 m_cp15;

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
        m_regs.r15 = m_baseVectorAddress + vector * 4;
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
        m_pipeline[0] = CodeReadWord(m_regs.r15 + 0);
        m_pipeline[1] = CodeReadWord(m_regs.r15 + 4);
        core::cycles_t cycles = 3;
        m_regs.r15 += 8;
        return cycles;
    }

    core::cycles_t ReloadPipelineTHUMB() {
        assert(m_regs.cpsr.t == 1);
        m_pipeline[0] = CodeReadHalf(m_regs.r15 + 0);
        m_pipeline[1] = CodeReadHalf(m_regs.r15 + 2);
        core::cycles_t cycles = 3;
        m_regs.r15 += 4;
        return cycles;
    }

    bool EvalCondition(const arm::ConditionFlags cond) const {
        if (cond >= arm::Cond_AL) {
            return true;
        }
        return s_conditionsTable[(m_regs.cpsr.u32 >> 28) | (cond << 4)];
    }

    uint32_t Shift(uint32_t value, uint8_t shiftOp, bool &carry, core::cycles_t &cycles) {
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

    uint32_t Shift(uint32_t value, uint8_t shiftOp, core::cycles_t &cycles) {
        bool carry = m_regs.cpsr.c;
        return Shift(value, shiftOp, carry, cycles);
    }

    // --- Memory accessors ---------------------------------------------------

    template <typename T>
    T CodeReadImpl(uint32_t address) {
        static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "CodeReadImpl requires uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        if (address < m_itcmReadSize) {
            return util::MemRead<T>(m_itcm, address & 0x7FFF);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return m_sys.MemReadHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return m_sys.MemReadWord(address);
        }
    }

    uint16_t CodeReadHalf(uint32_t address) {
        return CodeReadImpl<uint16_t>(address);
    }

    uint32_t CodeReadWord(uint32_t address) {
        return CodeReadImpl<uint32_t>(address);
    }

    template <typename T>
    bool DataReadImpl(uint32_t address, T &value) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataReadImpl requires uint8_t, uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        if (address < m_itcmReadSize) {
            value = util::MemRead<T>(m_itcm, address & 0x7FFF);
        } else if (address - m_dtcmBase < m_dtcmReadSize) {
            value = util::MemRead<T>(m_dtcm, (address - m_dtcmBase) & 0x3FFF);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            value = m_sys.MemReadByte(address);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            value = m_sys.MemReadHalf(address);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            value = m_sys.MemReadWord(address);
        }
        return true;
    }

    bool DataReadByte(uint32_t address, uint32_t &value) {
        uint8_t byteValue;
        if (DataReadImpl<uint8_t>(address, byteValue)) {
            value = byteValue;
            return true;
        } else {
            return false;
        }
    }

    bool DataReadHalf(uint32_t address, uint32_t &value) {
        uint16_t halfValue;
        if (DataReadImpl<uint16_t>(address, halfValue)) {
            value = halfValue;
            return true;
        } else {
            return false;
        }
    }

    bool DataReadWord(uint32_t address, uint32_t &value) {
        return DataReadImpl<uint32_t>(address, value);
    }

    template <typename T>
    bool DataWriteImpl(uint32_t address, T value) {
        static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>,
                      "DataWriteImpl requires uint8_t, uint16_t or uint32_t");
        address &= ~(static_cast<uint32_t>(sizeof(T)) - 1);
        if (address < m_itcmWriteSize) {
            util::MemWrite(m_itcm, address & 0x7FFF, value);
        } else if (address - m_dtcmBase < m_dtcmWriteSize) {
            util::MemWrite(m_dtcm, (address - m_dtcmBase) & 0x3FFF, value);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            m_sys.MemWriteByte(address, value);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            m_sys.MemWriteHalf(address, value);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            m_sys.MemWriteWord(address, value);
        }
        return true;
    }

    bool DataWriteByte(uint32_t address, uint8_t value) {
        return DataWriteImpl<uint8_t>(address, value);
    }

    bool DataWriteHalf(uint32_t address, uint16_t value) {
        return DataWriteImpl<uint16_t>(address, value);
    }

    bool DataWriteWord(uint32_t address, uint32_t value) {
        return DataWriteImpl<uint32_t>(address, value);
    }

    bool DataReadSignedByte(uint32_t address, int32_t &value) {
        uint32_t uValue = 0;
        if (DataReadByte(address, uValue)) {
            value = bit::sign_extend<8, int32_t>(uValue);
            return true;
        } else {
            return false;
        }
    }

    bool DataReadSignedHalf(uint32_t address, int32_t &value) {
        uint32_t uValue = 0;
        if (DataReadHalf(address & ~1, uValue)) {
            value = bit::sign_extend<16, int32_t>(uValue);
        } else {
            return false;
        }
        return true;
    }

    bool DataReadUnalignedHalf(uint32_t address, uint32_t &value) {
        return DataReadHalf(address, value);
    }

    bool DataReadUnalignedWord(uint32_t address, uint32_t &value) {
        if (DataReadWord(address, value)) {
            uint32_t offset = (address & 3) * 8;
            value = std::rotr(value, offset);
            return true;
        } else {
            return false;
        }
    }

    // --- ARM instruction handlers -------------------------------------------

    core::cycles_t _ARM_BranchAndExchange(uint32_t instr) {
        // BX
        uint8_t rn = instr & 0xF;
        uint32_t value = m_regs.regs[rn];
        return BranchAndExchange(value);
    }

    core::cycles_t _ARM_BranchAndLinkExchange(uint32_t instr) {
        // BLX
        uint8_t rn = instr & 0xF;
        uint32_t value = m_regs.regs[rn];
        m_regs.r14 = m_regs.r15 - 4;
        return BranchAndExchange(value);
    }

    template <bool l, bool switchToThumb>
    core::cycles_t _ARM_BranchAndBranchWithLink(uint32_t instr) {
        // B, BL, BLX
        uint32_t value = bit::sign_extend<24>(instr & 0xFFFFFF) << 2;
        if constexpr (l || switchToThumb) {
            m_regs.r14 = m_regs.r15 - 4;
        }
        if constexpr (switchToThumb) {
            if constexpr (l) {
                value |= 2;
            }
            m_regs.cpsr.t = 1;
        }
        m_regs.r15 += value;
        return switchToThumb ? ReloadPipelineTHUMB() : ReloadPipelineARM();
    }

    core::cycles_t _ARM_CountLeadingZeros(uint32_t instr) {
        // CLZ
        uint8_t rd = (instr >> 12) & 0xF;
        uint8_t rm = instr & 0xF;
        if (rd != 15) {
            m_regs.regs[rd] = std::countl_zero(m_regs.regs[rm]);
        }
        m_regs.r15 += 4;
        return 1; // 1S
    }

    template <bool i, uint8_t opcode, bool s>
    core::cycles_t _ARM_DataProcessing(uint32_t instr) {
        constexpr bool isComparison = (opcode & 0b1100) == 0b1000;
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
            if (rd != 15 || isComparison) {
                m_regs.cpsr.z = (result == 0);
                m_regs.cpsr.n = (result >> 31);
                m_regs.cpsr.c = carry;
                m_regs.cpsr.v = overflow;
            }
        }

        if (rd == 15 && !isComparison) {
            if constexpr (s) {
                m_regs.r15 &= (m_regs.cpsr.t ? ~1 : ~3);
                cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
            } else {
                m_regs.r15 &= ~3;
                cycles += ReloadPipelineARM();
            }
        } else {
            m_regs.r15 += 4;
            cycles += 1; // 1S to fetch next instruction
        }
        return cycles;
    }

    // PSR Transfer to register
    template <bool ps>
    core::cycles_t _ARM_MRS(uint32_t instr) {
        uint8_t rd = (instr >> 12) & 0b1111;

        if (rd != 15) {
            if constexpr (ps) {
                m_regs.regs[rd] = m_spsr->u32;
            } else {
                m_regs.regs[rd] = m_regs.cpsr.u32;
            }
        }

        m_regs.r15 += 4;
        return 2; // 1I to update PSR + 1S to fetch next instruction
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
            mask |= 0xFF000000; // (f)
        }
        if ((instr >> 18) & 1) {
            mask |= 0x00FF0000; // (s)
        }
        if ((instr >> 17) & 1) {
            mask |= 0x0000FF00; // (x)
        }
        if ((instr >> 16) & 1) {
            mask |= 0x000000FF; // (c)
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
        core::cycles_t cycles = ((instr >> 16) & 0b111) ? 2 : 0; // 2I to load anything but flags
        cycles += 1;                                             // 1S to fetch next instruction
        return cycles;
    }

    template <bool a, bool s>
    core::cycles_t _ARM_MultiplyAccumulate(uint32_t instr) {
        // MUL, MLA, MULS, MLAS
        uint8_t rd = (instr >> 16) & 0b1111;
        uint8_t rn = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t multiplier = m_regs.regs[rs];

        uint32_t result = m_regs.regs[rm] * multiplier;
        if constexpr (a) {
            // MLA and MLAS only
            result += m_regs.regs[rn];
        }
        if (rd != 15) {
            m_regs.regs[rd] = result;
        }

        if constexpr (s) {
            // (S) flag
            m_regs.cpsr.z = (result == 0);
            m_regs.cpsr.n = (result >> 31);
        }

        m_regs.r15 += 4;

        core::cycles_t cycles = s ? 3 : 1; // 1I base, additional 2I to store flags
        cycles += 1;                       // 1S to fetch next instruction
        return cycles;
    }

    template <bool u, bool a, bool s>
    core::cycles_t _ARM_MultiplyAccumulateLong(uint32_t instr) {
        // SMULL, UMULL, SMLAL, UMLAL
        // SMULLS, UMULLS, SMLALS, UMLALS
        uint8_t rdHi = (instr >> 16) & 0b1111;
        uint8_t rdLo = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t multiplier = m_regs.regs[rs];

        int64_t result;
        if constexpr (u) {
            // SMULL(S), SMLAL(S)
            int64_t multiplicand = bit::sign_extend<32, int64_t>(m_regs.regs[rm]);
            int64_t signedMultiplier = bit::sign_extend<32, int64_t>(multiplier);
            result = multiplicand * signedMultiplier;
            if constexpr (a) {
                // SMLAL(S)
                int64_t value = (uint64_t)m_regs.regs[rdLo] | ((uint64_t)m_regs.regs[rdHi] << 32ull);
                result += value;
            }
        } else {
            // UMULL(S), UMLAL(S)
            uint64_t unsignedResult = (uint64_t)m_regs.regs[rm] * (uint64_t)multiplier;
            result = static_cast<int64_t>(unsignedResult);
            if constexpr (a) {
                // UMLAL(S)
                uint64_t value = (uint64_t)m_regs.regs[rdLo] | ((uint64_t)m_regs.regs[rdHi] << 32ull);
                result += value;
            }
        }

        if (rdLo != 15) {
            m_regs.regs[rdLo] = result;
        }
        if (rdHi != 15) {
            m_regs.regs[rdHi] = result >> 32ull;
        }

        if constexpr (s) {
            // (S) flag
            m_regs.cpsr.z = (result == 0);
            m_regs.cpsr.n = (result >> 63ull);
        }

        m_regs.r15 += 4;
        core::cycles_t cycles = s ? 4 : 2; // 2I base, additional 2I to store flags
        cycles += 1;                       // 1S to fetch next instruction
        return cycles;
    }

    template <bool y, bool x>
    core::cycles_t _ARM_SignedMultiply(uint32_t instr) {
        // SMULxy (SMULBB, SMULBT, SMULTB, SMULTT)
        uint8_t rd = (instr >> 16) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        if (rd != 15) {
            uint32_t &dst = m_regs.regs[rd];
            int16_t multiplicand = (m_regs.regs[rm] >> (x ? 16u : 0u));
            int16_t multiplier = (m_regs.regs[rs] >> (y ? 16u : 0u));

            dst = multiplicand * multiplier;
        }

        m_regs.r15 += 4;
        return 1; // 1S to fetch next instruction
    }

    template <bool y, bool x>
    core::cycles_t _ARM_SignedMultiplyAccumulate(uint32_t instr) {
        // SMLAxy (SMLABB, SMLABT, SMLATB, SMLATT)
        uint8_t rd = (instr >> 16) & 0b1111;
        uint8_t rn = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t &dst = m_regs.regs[rd];
        int16_t multiplicand = (m_regs.regs[rm] >> (x ? 16u : 0u));
        int16_t multiplier = (m_regs.regs[rs] >> (y ? 16u : 0u));
        int32_t accumulate = m_regs.regs[rn];

        int64_t result = (int64_t)multiplicand * multiplier + accumulate;
        int32_t result32 = result;
        m_regs.cpsr.q |= (result32 != result);
        if (rd != 15) {
            dst = result;
        }

        m_regs.r15 += 4;
        return 1; // 1S to fetch next instruction
    }

    template <bool y>
    core::cycles_t _ARM_SignedMultiplyWord(uint32_t instr) {
        // SMULWy (SMULWB, SMULWT)
        uint8_t rd = (instr >> 16) & 0b1111;
        if (rd != 15) {
            uint8_t rs = (instr >> 8) & 0b1111;
            uint8_t rm = (instr >> 0) & 0b1111;

            uint32_t &dst = m_regs.regs[rd];
            int32_t multiplicand = m_regs.regs[rm];
            int16_t multiplier = (m_regs.regs[rs] >> (y ? 16u : 0u));

            int64_t result = (int64_t)multiplicand * multiplier;
            dst = (result >> 16ll);
        }

        m_regs.r15 += 4;
        return 1; // 1S to fetch next instruction
    }

    template <bool y>
    core::cycles_t _ARM_SignedMultiplyAccumulateWord(uint32_t instr) {
        // SMLAWx (SMLAWB, SMLAWT)
        uint8_t rd = (instr >> 16) & 0b1111;
        uint8_t rn = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t &dst = m_regs.regs[rd];
        int32_t multiplicand = m_regs.regs[rm];
        int16_t multiplier = (m_regs.regs[rs] >> (y ? 16u : 0u));
        int32_t accumulate = m_regs.regs[rn];

        int64_t result = (((int64_t)multiplicand * multiplier) >> 16) + accumulate;
        int32_t result32 = result;
        m_regs.cpsr.q |= (result32 != result);
        if (rd != 15) {
            dst = result;
        }

        m_regs.r15 += 4;
        return 1; // 1S to fetch next instruction
    }

    template <bool y, bool x>
    core::cycles_t _ARM_SignedMultiplyAccumulateLong(uint32_t instr) {
        // SMLALxy (SMLALBB, SMLALBT, SMLALTB, SMLALTT)
        uint8_t rdHi = (instr >> 16) & 0b1111;
        uint8_t rdLo = (instr >> 12) & 0b1111;
        uint8_t rs = (instr >> 8) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        uint32_t &dstHi = m_regs.regs[rdHi];
        uint32_t &dstLo = m_regs.regs[rdLo];
        int16_t multiplicand = (m_regs.regs[rm] >> (x ? 16u : 0u));
        int16_t multiplier = (m_regs.regs[rs] >> (y ? 16u : 0u));
        int64_t accumulate = dstLo | ((uint64_t)dstHi << 32ull);

        int64_t result = (int64_t)multiplicand * multiplier + accumulate;
        dstLo = (result >> 0ull);
        dstHi = (result >> 32ull);

        m_regs.r15 += 4;
        return 2; // 1I for extra calculations + 1S to fetch next instruction
    }

    template <bool dbl, bool sub>
    core::cycles_t _ARM_EnhancedDSPAddSub(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        int64_t op1 = bit::sign_extend<32, int64_t>(m_regs.regs[rm]);
        int64_t op2 = bit::sign_extend<32, int64_t>(m_regs.regs[rn]);
        uint32_t &dst = m_regs.regs[rd];
        int32_t result;

        if constexpr (dbl) {
            // QDADD/QDSUB double the value of op2 and set Q if the new value was saturated
            int32_t dblOp2;
            m_regs.cpsr.q |= arm::Saturate(op2 + op2, dblOp2);
            op2 = dblOp2;
        }

        if constexpr (sub) {
            // QSUB/QDSUB
            m_regs.cpsr.q |= arm::Saturate(op1 - op2, result);
        } else {
            // QADD/QDADD
            m_regs.cpsr.q |= arm::Saturate(op1 + op2, result);
        }
        if (rd != 15) {
            dst = result;
        }

        m_regs.r15 += 4;
        return 1; // 1S to fetch next instruction
    }

    template <bool i, bool p, bool u, bool b, bool w, bool l>
    core::cycles_t _ARM_SingleDataTransfer(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint16_t offset = (instr & 0xFFF);

        // When the W bit is set in a post-indexed operation, the transfer affects user mode registers
        constexpr bool userModeTransfer = (w && !p);

        core::cycles_t cycles = 0;

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
        auto &dst = userModeTransfer ? m_regs.UserModeGPR(rd) : m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (b && l) {
            // LDRB
            dataAccessOK = DataReadByte(address, dst);
        } else if constexpr (b) {
            // STRB
            dataAccessOK = DataWriteByte(address, dst + ((rd == 15) ? 4 : 0));
        } else if constexpr (l) {
            // LDR
            dataAccessOK = (rd == 15) ? DataReadWord(address, dst) : DataReadUnalignedWord(address, dst);
        } else {
            // STR
            dataAccessOK = DataWriteWord(address, dst + ((rd == 15) ? 4 : 0));
        }

        if (dataAccessOK) {
            // Write back address if requested
            if (!l || rn != rd) {
                if constexpr (!p) {
                    m_regs.regs[rn] += (u ? offsetValue : -offsetValue);
                } else if constexpr (w) {
                    m_regs.regs[rn] = address;
                }
            }
        }

        // Update PC
        if ((l && rd == 15) || ((!l || rn != rd) && (!p || w) && (rn == 15))) {
            cycles++;    // 1I
            cycles += 1; // 1N data cycle happens during an 1I code cycle
            if (dataAccessOK) {
                if (!m_cp15.ctl.preARMv5) {
                    // Switch to THUMB mode if bit 0 is set (ARMv5 feature)
                    m_regs.cpsr.t = (m_regs.r15 & 1);
                }
                m_regs.r15 &= m_regs.cpsr.t ? ~1 : ~3;
                cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
            } else {
                // TODO: check timing
                cycles += EnterException(arm::Excpt_DataAbort);
            }
        } else {
            if (dataAccessOK) {
                m_regs.r15 += 4;
                // N data cycle happens in parallel with the S code cycle to fetch next instruction
                cycles += 1;
            } else {
                // TODO: check timing
                cycles += EnterException(arm::Excpt_DataAbort);
            }
        }
        return cycles;
    }

    template <bool p, bool u, bool i, bool w, bool l, bool s, bool h>
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

        core::cycles_t cycles = 0;

        // Perform the transfer
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK = true;
        if constexpr (l) {
            if constexpr (s && h) {
                // LDRSH
                dataAccessOK = DataReadSignedHalf(address, reinterpret_cast<int32_t &>(dst));
            } else if constexpr (s) {
                // LDRSB
                dataAccessOK = DataReadSignedByte(address, reinterpret_cast<int32_t &>(dst));
            } else if constexpr (h) {
                // LDRH
                dataAccessOK = DataReadUnalignedHalf(address, dst);
            } else {
                return EnterException(arm::Excpt_UndefinedInstruction);
            }
        } else {
            if constexpr (s && h) {
                // STRD
                if (rd & 1) {
                    // Undefined instruction
                    return EnterException(arm::Excpt_UndefinedInstruction);
                } else {
                    // 1S is handled below
                    dataAccessOK = DataWriteWord(address + 0, m_regs.regs[rd + 0]) &&
                                   DataWriteWord(address + 4, m_regs.regs[rd + 1] + (rd == 14 ? 4 : 0));
                }
            } else if constexpr (s) {
                // LDRD
                if (rd & 1) {
                    // Undefined instruction
                    return EnterException(arm::Excpt_UndefinedInstruction);
                } else {
                    // 1S is handled below
                    dataAccessOK = DataReadUnalignedWord(address + 0, dst) &&
                                   DataReadUnalignedWord(address + 4, m_regs.regs[rd + 1]);
                    if (rd == 14) {
                        m_regs.r15 &= ~1; // LDRD never switches to THUMB mode
                    }
                }
            } else if constexpr (h) {
                // STRH
                uint32_t value = m_regs.regs[rd] + (rd == 15 ? 4 : 0);
                dataAccessOK = DataWriteHalf(address, value);
            } else {
                return EnterException(arm::Excpt_UndefinedInstruction);
            }
        }

        if (dataAccessOK) {
            // Write back address if requested
            if ((l && rn != rd) ||                 // LDRSH, LDRSB, LDRH
                (!l && s && !h && rn != rd + 1) || // LDRD
                (!l && h)                          // STRD, STRH
            ) {
                if constexpr (!p) {
                    m_regs.regs[rn] = address + (u ? offsetValue : -offsetValue);
                } else if constexpr (w) {
                    m_regs.regs[rn] = address;
                }
            }
        }

        // Update PC
        if ((l && rd == 15) || ((!l || rn != rd) && (!p || w) && rn == 15) || (!l && s && !h && rd == 14)) {
            cycles++;    // 1I
            cycles += 1; // N data cycle happens during an I code cycle
            if (dataAccessOK) {
                if constexpr (l || !s || h) {
                    // For non-LDRD instructions, honor CP15 bit L4
                    if (!m_cp15.ctl.preARMv5) {
                        // Switch to THUMB mode if bit 0 is set (ARMv5 feature)
                        m_regs.cpsr.t = (m_regs.r15 & 1);
                    }
                }
                m_regs.r15 &= m_regs.cpsr.t ? ~1 : ~3;
                cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
            } else {
                // TODO: check timing
                cycles += EnterException(arm::Excpt_DataAbort);
            }
        } else {
            if (dataAccessOK) {
                m_regs.r15 += 4;
            }
            if constexpr (!l && s) {
                // Handle LDRD and STRD
                // N data cycle happens during an I code cycle
                cycles += 1;
                if (dataAccessOK) {
                    // S data cycle happens in parallel with the S code cycle to fetch next instruction
                    cycles += 1;
                } else {
                    // TODO: check timing
                    cycles += EnterException(arm::Excpt_DataAbort);
                }
            } else {
                if (dataAccessOK) {
                    // N data cycle happens in parallel with the S code cycle to fetch next instruction
                    cycles += 1;
                } else {
                    // TODO: check timing
                    cycles += EnterException(arm::Excpt_DataAbort);
                }
            }
        }
        return cycles;
    }

    template <bool p, bool u, bool s, bool w, bool l>
    core::cycles_t _ARM_BlockDataTransfer(uint32_t instr) {
        // LDM, STM
        uint8_t rn = (instr >> 16) & 0b1111;
        uint16_t regList = (instr & 0xFFFF);

        uint32_t address = m_regs.regs[rn];
        bool pcIncluded = regList & (1 << 15);
        bool userModeTransfer = s && (!l || !pcIncluded);
        [[maybe_unused]] arm::Mode currMode = m_regs.cpsr.mode;

        // Get first register and compute total transfer size
        uint32_t firstReg;
        uint32_t lastReg;
        uint32_t size;
        if (regList == 0) {
            // An empty list results in transferring nothing but incrementing the address as if we had a full list
            firstReg = 17;
            lastReg = 16;
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

        core::cycles_t cycles = 0;
        core::cycles_t dataCycles = 0;
        core::cycles_t lastDataCycles = 0;
        bool dataAccessOK = true;

        // Execute transfer
        for (uint32_t i = firstReg; i <= lastReg; i++) {
            if (~regList & (1 << i)) {
                continue;
            }

            if constexpr (preInc) {
                address += 4;
            }

            // Transfer data
            if (dataAccessOK) {
                if constexpr (l) {
                    uint32_t &reg = userModeTransfer ? m_regs.UserModeGPR(i) : m_regs.regs[i];
                    dataAccessOK = DataReadWord(address, reg);
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
                        value = userModeTransfer ? m_regs.UserModeGPR(i) : m_regs.regs[i];
                    }
                    dataAccessOK = DataWriteWord(address, value);
                }
            }
            if (i == firstReg) {
                // 1N for the first access
                dataCycles += 1;
            } else {
                // 1S for each subsequent access
                lastDataCycles = 1;
                dataCycles += lastDataCycles;
            }

            if constexpr (!preInc) {
                address += 4;
            }
        }

        if constexpr (w) {
            if (dataAccessOK) {
                // STMs always writeback
                // LDMs writeback only if Rn is not the last in the register list, or if it's the only register in the
                // list
                if (!l || lastReg != rn || regList == (1 << rn)) {
                    if (l && s && pcIncluded) {
                        m_regs.GPR(rn, currMode) = finalAddress;
                    } else {
                        m_regs.regs[rn] = finalAddress;
                    }
                }
            }
        }

        // Update PC
        if ((l && pcIncluded) || ((w && (!l || lastReg != rn || regList == (1 << rn))) && rn == 15)) {
            cycles++;             // 1I
            cycles += dataCycles; // Data cycles happen during I code cycles
            if (dataAccessOK) {
                if (!m_cp15.ctl.preARMv5) {
                    // Switch to THUMB mode if bit 0 is set (ARMv5 feature)
                    m_regs.cpsr.t = (m_regs.r15 & 1);
                }
                m_regs.r15 &= m_regs.cpsr.t ? ~1 : ~3;
                cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
            } else {
                cycles += EnterException(arm::Excpt_DataAbort);
            }
        } else {
            if (dataAccessOK) {
                m_regs.r15 += 4;
            }
            if (firstReg == lastReg) {
                // Only one register was transferred
                // N data cycle happens during an I code cycle
                cycles += dataCycles;
                if (dataAccessOK) {
                    // S code cycle to fetch next instruction happens during an I data cycle
                    cycles += 1;
                } else {
                    // TODO: check timing
                    cycles += EnterException(arm::Excpt_DataAbort);
                }
            } else {
                // More than one register was transferred
                // All but the last data cycle happen during I code cycles
                cycles += dataCycles - lastDataCycles;
                if (dataAccessOK) {
                    // The last S data cycle happens during the S code cycle to fetch next instruction
                    cycles += std::max<core::cycles_t>(lastDataCycles, 1);
                } else {
                    // TODO: check timing
                    cycles += EnterException(arm::Excpt_DataAbort);
                }
            }
        }
        return cycles;
    }

    template <bool b>
    core::cycles_t _ARM_SingleDataSwap(uint32_t instr) {
        uint8_t rn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t rm = (instr >> 0) & 0b1111;

        auto address = m_regs.regs[rn];
        auto src = m_regs.regs[rm];
        auto &dst = m_regs.regs[rd];

        bool dataAccessOK = true;

        // Perform the swap
        uint32_t tmp;
        if constexpr (b) {
            dataAccessOK = DataReadByte(address, tmp) && //
                           DataWriteByte(address, src);
        } else {
            dataAccessOK = DataReadUnalignedWord(address, tmp) && //
                           DataWriteWord(address, src);
        }
        if (dataAccessOK && rd != 15) {
            dst = tmp;
        }

        // First N data cycle happens during an I code cycle
        // Second N data cycle happens during an S code cycle to fetch next instruction

        if (dataAccessOK) {
            m_regs.r15 += 4;
            return 2;
        } else {
            // TODO: check timing
            return 2 + EnterException(arm::Excpt_DataAbort);
        }
    }

    // Software Interrupt
    core::cycles_t _ARM_SoftwareInterrupt(uint32_t instr) {
        // uint32_t comment = (instr & 0xFFFFFF);
        return EnterException(arm::Excpt_SoftwareInterrupt);
    }

    core::cycles_t _ARM_SoftwareBreakpoint(uint32_t instr) {
        // uint16_t immedHi = (instr >> 8) & 0xFFF;
        // uint16_t immedLo = (instr & 0xF);
        // uint16_t immed = (immedLo) | (immedHi << 4);
        return EnterException(arm::Excpt_PrefetchAbort);
    }

    // template <bool i, bool u>
    core::cycles_t _ARM_Preload(uint32_t instr) {
        // p is always true
        // w is always false
        // uint16_t rn = (instr >> 16) & 0xF;
        // uint16_t addrMode = instr & 0xFFF;
        m_regs.r15 += 4;
        return 2;
    }

    // CDP  when cond != 0b1111
    // CDP2 when cond == 0b1111
    // template <uint8_t opcode1, uint8_t opcode2>
    core::cycles_t _ARM_CopDataOperations(uint32_t instr) {
        // uint8_t crn = (instr >> 16) & 0b1111;
        // uint8_t crd = (instr >> 12) & 0b1111;
        // uint8_t cpnum = (instr >> 8) & 0b1111;
        // uint8_t crm = (instr >> 0) & 0b1111;
        return EnterException(arm::Excpt_UndefinedInstruction);
    }

    // STC  when !l and cond != 0b1111
    // STC2 when !l and cond == 0b1111
    // LDC  when  l and cond != 0b1111
    // LDC2 when  l and cond == 0b1111
    // template <bool p, bool u, bool n, bool w, bool l>
    core::cycles_t _ARM_CopDataTransfer(uint32_t instr) {
        // uint8_t rn = (instr >> 16) & 0b1111;
        // uint8_t crd = (instr >> 12) & 0b1111;
        // uint8_t cpnum = (instr >> 8) & 0b1111;
        // uint8_t offset = (instr >> 0) & 0b1111'1111;
        return EnterException(arm::Excpt_UndefinedInstruction);
    }

    // MCR  when !s and cond != 0b1111
    // MCR2 when !s and cond == 0b1111
    // MRC  when  s and cond != 0b1111
    // MRC2 when  s and cond == 0b1111
    template <uint8_t opcode1, bool s, uint16_t opcode2>
    core::cycles_t _ARM_CopRegTransfer(uint32_t instr) {
        uint16_t crn = (instr >> 16) & 0b1111;
        uint8_t rd = (instr >> 12) & 0b1111;
        uint8_t cpnum = (instr >> 8) & 0b1111;
        uint16_t crm = (instr >> 0) & 0b1111;

        // Non-existent coprocessor results in undefined instruction exception
        if (cpnum != 15) {
            return EnterException(arm::Excpt_UndefinedInstruction);
        }

        // CP15 doesn't support MCR2/MRC2
        if ((instr >> 28) == 0xF) {
            return EnterException(arm::Excpt_UndefinedInstruction);
        }

        // PC is incremented before it is transferred to the coprocessor
        m_regs.r15 += 4;

        if constexpr (s) {
            uint32_t data;
            if (cpnum == 15 && opcode1 == 0) {
                data = CP15Read((crn << 8) | (crm << 4) | opcode2);
            } else {
                data = 0;
            }
            if (rd == 15) {
                // Update NZCV flags
                m_regs.cpsr.u32 = (m_regs.cpsr.u32 & 0x0FFFFFFF) | (data & 0xF0000000);
            } else {
                m_regs.regs[rd] = data;
            }
        } else {
            uint32_t data = m_regs.regs[rd];
            if (cpnum == 15 && opcode1 == 0) {
                CP15Write((crn << 8) | (crm << 4) | opcode2, data);
            }
        }

        return 1; // 1S to fetch next instruction
    }

    core::cycles_t _ARM_UndefinedInstruction(uint32_t instr) {
        return 1 + EnterException(arm::Excpt_UndefinedInstruction); // 1I + 1S to fetch next instruction
    }

    core::cycles_t _ARM_Unmapped(uint32_t instr) {
        throw std::runtime_error("Unmapped ARM instruction");
    }

    // --- THUMB instruction handlers -----------------------------------------

    template <uint8_t op, uint8_t offset>
    core::cycles_t _THUMB_MoveShiftedRegister(uint16_t instr) {
        // LSL, LSR, ASR
        uint8_t rs = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        auto &dst = m_regs.regs[rd];
        bool carry = m_regs.cpsr.c;
        dst = arm::CalcImmShift<static_cast<arm::ShiftOp>(op)>(m_regs.regs[rs], offset, carry);
        m_regs.cpsr.z = (dst == 0);
        m_regs.cpsr.n = (dst >> 31);
        m_regs.cpsr.c = carry;

        m_regs.r15 += 2;
        return 2; // 1I due to use of register specified shift
                  // 1S to fetch next instruction
    }

    template <bool i, bool op, uint8_t rnOrOffset>
    core::cycles_t _THUMB_AddSub(uint16_t instr) {
        // ADD, SUB
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
        return 1; // 1S to fetch next instruction
    }

    template <uint8_t op, uint8_t rd>
    core::cycles_t _THUMB_MovCmpAddSubImmediate(uint16_t instr) {
        // MOV, CMP, ADD, SUB
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
        return 1; // 1S to fetch next instruction
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
            result = dst *= src;
            cycles += 3; // 3I for multiplication with flag store
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
        return cycles + 1; // 1S to fetch next instruction
    }

    template <uint8_t op, bool h1, bool h2>
    core::cycles_t _THUMB_HiRegOperations(uint16_t instr) {
        // ADD, CMP, MOV, BX, BLX
        uint8_t rshs = ((instr >> 3) & 0b111) + (h2 ? 8 : 0);
        uint8_t rdhd = ((instr >> 0) & 0b111) + (h1 ? 8 : 0);

        auto &src = m_regs.regs[rshs];
        auto &dst = m_regs.regs[rdhd];
        if constexpr (op == 0b11) {
            uint32_t addr = src;
            if constexpr (h1) {
                // BLX
                m_regs.r14 = (m_regs.r15 - 2) | 1;
            }
            // else: BX
            return BranchAndExchange(addr);
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
                return 1; // 1S to fetch next instruction
            }
        }
    }

    template <uint8_t rd>
    core::cycles_t _THUMB_PCRelativeLoad(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;
        uint32_t address = (m_regs.r15 & ~3) + offset;
        bool dataAccessOK = DataReadWord(address, m_regs.regs[rd]);

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool l, bool b, uint8_t ro>
    core::cycles_t _THUMB_LoadStoreRegOffset(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + m_regs.regs[ro];
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (l && b) {
            // LDRB
            dataAccessOK = DataReadByte(address, dst);
        } else if constexpr (l) {
            // LDR
            dataAccessOK = DataReadUnalignedWord(address, dst);
        } else if constexpr (b) {
            // STRB
            dataAccessOK = DataWriteByte(address, dst);
        } else {
            // STR
            dataAccessOK = DataWriteWord(address, dst);
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool h, bool s, uint8_t ro>
    core::cycles_t _THUMB_LoadStoreSignExtended(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + m_regs.regs[ro];
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (h && s) {
            // LDRSH
            dataAccessOK = DataReadSignedHalf(address, reinterpret_cast<int32_t &>(dst));
        } else if constexpr (h) {
            // LDRH
            dataAccessOK = DataReadUnalignedHalf(address, dst);
        } else if constexpr (s) {
            // LDRSB
            dataAccessOK = DataReadSignedByte(address, reinterpret_cast<int32_t &>(dst));
        } else {
            // STRH
            dataAccessOK = DataWriteHalf(address, dst);
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool b, bool l, uint16_t offset>
    core::cycles_t _THUMB_LoadStoreImmOffset(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + offset;
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (b && l) {
            // LDRB
            dataAccessOK = DataReadByte(address, dst);
        } else if constexpr (b) {
            // STRB
            dataAccessOK = DataWriteByte(address, dst);
        } else if constexpr (l) {
            // LDR
            dataAccessOK = DataReadUnalignedWord(address, dst);
        } else {
            // STR
            dataAccessOK = DataWriteWord(address, dst);
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool l, uint16_t offset>
    core::cycles_t _THUMB_LoadStoreHalfWord(uint16_t instr) {
        uint8_t rb = (instr >> 3) & 0b111;
        uint8_t rd = (instr >> 0) & 0b111;

        // Perform the transfer
        uint32_t address = m_regs.regs[rb] + offset;
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (l) {
            // LDRH
            dataAccessOK = DataReadUnalignedHalf(address, dst);
        } else {
            // STRH
            dataAccessOK = DataWriteHalf(address, dst);
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool l, uint8_t rd>
    core::cycles_t _THUMB_SPRelativeLoadStore(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;

        // Perform the transfer
        uint32_t address = m_regs.r13 + offset;
        auto &dst = m_regs.regs[rd];
        bool dataAccessOK;
        if constexpr (l) {
            dataAccessOK = DataReadUnalignedWord(address, dst);
        } else {
            dataAccessOK = DataWriteWord(address, dst);
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
            // 1N data cycle happens in parallel with the 1S code cycle to fetch next instruction
            return 1;
        } else {
            // TODO: check timing
            return EnterException(arm::Excpt_DataAbort);
        }
    }

    template <bool sp, uint8_t rd>
    core::cycles_t _THUMB_LoadAddress(uint16_t instr) {
        uint16_t offset = (instr & 0xFF) << 2;
        m_regs.regs[rd] = (sp ? m_regs.r13 : (m_regs.r15 & ~3)) + offset;

        m_regs.r15 += 2;
        return 1; // 1S to fetch next instruction
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
        return 1; // 1S to fetch next instruction
    }

    template <bool l, bool r>
    core::cycles_t _THUMB_PushPopRegs(uint16_t instr) {
        uint8_t regList = (instr & 0xFF);
        uint32_t address = m_regs.r13;

        core::cycles_t cycles = 0;
        core::cycles_t dataCycles = 0;
        core::cycles_t lastDataCycles = 0;
        bool dataAccessOK = true;

        auto tickAccess = [&](bool write) {
            lastDataCycles = 1;
            dataCycles += lastDataCycles;
        };
        auto push = [&](uint32_t value) {
            if (dataAccessOK) {
                dataAccessOK = DataWriteWord(address, value);
            }
            tickAccess(true);
            address += 4;
        };
        auto pop = [&](uint32_t &value) {
            if (dataAccessOK) {
                dataAccessOK = DataReadWord(address, value);
            }
            tickAccess(false);
            address += 4;
        };

        if constexpr (l) {
            // Pop registers
            for (uint32_t i = 0; i < 8; i++) {
                if (regList & (1 << i)) {
                    pop(m_regs.regs[i]);
                }
            }

            // Pop PC if requested
            if constexpr (r) {
                pop(m_regs.r15);
                if (dataAccessOK) {
                    if (!m_cp15.ctl.preARMv5) {
                        // Switch to ARM mode if bit 0 is clear (ARMv5 feature)
                        m_regs.cpsr.t = (m_regs.r15 & 1);
                    }
                    m_regs.r15 &= m_regs.cpsr.t ? ~1 : ~3;
                    cycles += m_regs.cpsr.t ? ReloadPipelineTHUMB() : ReloadPipelineARM();
                }
                cycles++; // 1I for popping PC
            }

            // Update SP
            if (dataAccessOK) {
                m_regs.r13 = address;
            }
        } else {
            // Precompute address
            address -= std::popcount(regList) * 4;
            if constexpr (r) {
                address -= 4;
            }

            // Update SP
            if (dataAccessOK) {
                m_regs.r13 = address;
            }

            // Push registers
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
            if (dataAccessOK) {
                m_regs.r15 += 2;
            }
            if (std::popcount(regList) == 1) {
                // Only one register was transferred
                // N data cycle happens during an I code cycle
                cycles += dataCycles;
                if (dataAccessOK) {
                    // S code cycle to fetch next instruction happens during an I data cycle
                    cycles += 1;
                } else {
                    // TODO: check timing
                    return EnterException(arm::Excpt_DataAbort);
                }
            } else {
                // More than one register was transferred
                // All but the last data cycle happen during I code cycles
                cycles += dataCycles - lastDataCycles;
                if (dataAccessOK) {
                    // The last S data cycle happens during the S code cycle to fetch next instruction
                    cycles += std::max<core::cycles_t>(lastDataCycles, 1);
                } else {
                    // TODO: check timing
                    return EnterException(arm::Excpt_DataAbort);
                }
            }
        }
        return cycles;
    }

    template <bool l, uint8_t rb>
    core::cycles_t _THUMB_MultipleLoadStore(uint16_t instr) {
        auto address = m_regs.regs[rb];
        uint8_t regList = (instr & 0xFF);

        // An empty list results in transferring nothing but incrementing the address as if we had a full list
        if (regList == 0) {
            m_regs.regs[rb] = address + 0x40;
            m_regs.r15 += 2;
            return 1;
        }

        uint8_t firstReg = std::countr_zero(regList);
        uint8_t lastReg = 7 - std::countl_zero(regList);

        core::cycles_t dataCycles = 0;
        core::cycles_t lastDataCycles = 0;
        bool dataAccessOK = true;

        auto tickAccess = [&](uint32_t addr, bool write) {
            lastDataCycles = 1;
            dataCycles += lastDataCycles;
        };

        if constexpr (l) {
            auto load = [&](uint32_t &value) {
                if (dataAccessOK) {
                    dataAccessOK = DataReadWord(address, value);
                }
                tickAccess(address, false);
                address += 4;
            };

            for (uint32_t i = firstReg; i <= lastReg; i++) {
                if (regList & (1 << i)) {
                    load(m_regs.regs[i]);
                }
            }

            // Writeback address if the register is not in the list
            if (dataAccessOK) {
                if ((regList & (1 << rb)) == 0) {
                    m_regs.regs[rb] = address;
                }
            }
        } else {
            auto store = [&](uint32_t value) {
                if (dataAccessOK) {
                    dataAccessOK = DataWriteWord(address, value);
                }
                tickAccess(address, true);
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
            if (dataAccessOK) {
                m_regs.regs[rb] = address;
            }
        }

        if (dataAccessOK) {
            m_regs.r15 += 2;
        }
        core::cycles_t cycles = 0;
        if (firstReg == lastReg) {
            // Only one register was transferred
            // N data cycle happens during an I code cycle
            cycles += dataCycles;
            if (dataAccessOK) {
                // S code cycle to fetch next instruction happens during an I data cycle
                cycles += 1;
            } else {
                // TODO: check timing
                return EnterException(arm::Excpt_DataAbort);
            }
        } else {
            // More than one register was transferred
            // All but the last data cycle happen during I code cycles
            cycles += dataCycles - lastDataCycles;
            if (dataAccessOK) {
                // The last S data cycle happens during the S code cycle to fetch next instruction
                cycles += std::max<core::cycles_t>(lastDataCycles, 1);
            } else {
                // TODO: check timing
                return EnterException(arm::Excpt_DataAbort);
            }
        }
        return cycles;
    }

    template <arm::ConditionFlags cond>
    core::cycles_t _THUMB_ConditionalBranch(uint16_t instr) {
        // B<cond>
        if (EvalCondition(cond)) {
            int32_t offset = bit::sign_extend<8, int32_t>(instr & 0xFF) << 1;
            m_regs.r15 += offset;
            return ReloadPipelineTHUMB();
        } else {
            m_regs.r15 += 2;
            return 1; // 1S to fetch next instruction
        }
    }

    core::cycles_t _THUMB_SoftwareInterrupt(uint16_t instr) {
        // SWI
        // uint8_t comment = (instr & 0xFF);
        return EnterException(arm::Excpt_SoftwareInterrupt);
    }

    core::cycles_t _THUMB_SoftwareBreakpoint(uint16_t instr) {
        // BKPT
        // uint8_t immed = (instr & 0xFF);
        return EnterException(arm::Excpt_PrefetchAbort);
    }

    core::cycles_t _THUMB_UndefinedInstruction(uint16_t instr) {
        return 1 + EnterException(arm::Excpt_UndefinedInstruction); // 1I + instruction fetch
    }

    core::cycles_t _THUMB_UnconditionalBranch(uint16_t instr) {
        // B
        int32_t offset = bit::sign_extend<11, int32_t>(instr & 0x7FF) << 1;
        m_regs.r15 += offset;
        return ReloadPipelineTHUMB();
    }

    template <uint8_t h>
    core::cycles_t _THUMB_LongBranchWithLink(uint16_t instr) {
        // BL, BLX
        if constexpr (h == 0b01) {
            if (instr & 1) {
                return 1 + EnterException(arm::Excpt_UndefinedInstruction); // 1I + instruction fetch
            }
        }

        uint32_t offset = (instr & 0x7FF);
        auto &lr = m_regs.r14;
        auto &pc = m_regs.r15;
        if constexpr (h == 0b11) {
            // BL suffix
            uint32_t nextAddr = pc - 2;
            pc = (lr + (offset << 1)) & ~1;
            lr = nextAddr | 1;
            return ReloadPipelineTHUMB();
        } else if constexpr (h == 0b10) {
            // BL/BLX prefix
            lr = pc + bit::sign_extend<23>(offset << 12);
            pc += 2;
            return 1; // 1S to fetch next instruction
        } else if constexpr (h == 0b01) {
            // BLX suffix
            uint32_t nextAddr = pc - 2;
            pc = (lr + (offset << 1)) & ~3;
            lr = nextAddr | 1;
            m_regs.cpsr.t = 0;
            return ReloadPipelineARM();
        }
    }

    core::cycles_t _THUMB_Unmapped(uint16_t instr) {
        throw std::runtime_error("Unmapped THUMB instruction");
    }

    // --- CP15 ---------------------------------------------------------------

    void ConfigureDTCM() {
        if (m_cp15.ctl.dtcmEnable) {
            m_dtcmBase = m_cp15.dtcmParams & 0xFFFFF000;
            m_dtcmWriteSize = 0x200 << ((m_cp15.dtcmParams >> 1) & 0x1F);
            m_dtcmReadSize = m_cp15.ctl.dtcmLoad ? 0 : m_dtcmWriteSize;
        } else {
            m_dtcmBase = 0xFFFFFFFF;
            m_dtcmWriteSize = m_dtcmReadSize = 0;
        }
    }

    void ConfigureITCM() {
        if (m_cp15.ctl.itcmEnable) {
            m_itcmWriteSize = 0x200 << ((m_cp15.itcmParams >> 1) & 0x1F);
            m_itcmReadSize = m_cp15.ctl.itcmLoad ? 0 : m_itcmWriteSize;
        } else {
            m_itcmWriteSize = m_itcmReadSize = 0;
        }
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
                case arm::Cond_NV:
                    entry = true;
                    break; // Special instructions that run unconditionally, not exactly "never"
                }
            }
        }
        return arr;
    }();

    template <auto MemberFunc>
    static core::cycles_t ARMInstrHandlerWrapper(ARM946ES &instance, uint32_t instr) {
        return (instance.*MemberFunc)(instr);
    }

    template <auto MemberFunc>
    static core::cycles_t THUMBInstrHandlerWrapper(ARM946ES &instance, uint16_t instr) {
        return (instance.*MemberFunc)(instr);
    }

    template <uint32_t instr, bool specialCond>
    static constexpr auto MakeARMInstruction() {
        const auto op = (instr >> 9);
        if constexpr (specialCond) {
            if constexpr (op == 0b000 || op == 0b001 || op == 0b100) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
            } else if constexpr (op == 0b010 || op == 0b011) {
                if constexpr ((instr & 0b1'0111'0000) == 0b1'0101'0000) {
                    // const bool i = (instr >> 9) & 1;
                    // const bool u = (instr >> 7) & 1;
                    // return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_Preload<i, u>>;
                    return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_Preload>;
                } else {
                    return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
                }
            } else if constexpr (op == 0b111) {
                if ((instr >> 8) & 1) {
                    return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
                }
            }
        }
        if constexpr (op == 0b000) {
            if constexpr ((instr & 0b1'1100'1111) == 0b0'0000'1001) {
                const bool a = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_MultiplyAccumulate<a, s>>;
            } else if constexpr ((instr & 0b1'1000'1111) == 0b0'1000'1001) {
                const bool u = (instr >> 6) & 1;
                const bool a = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_MultiplyAccumulateLong<u, a, s>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0000'1001) {
                const bool b = (instr >> 6) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SingleDataSwap<b>>;
            } else if constexpr ((instr & 0b1001) == 0b1001) {
                const bool p = (instr >> 8) & 1;
                const bool u = (instr >> 7) & 1;
                const bool i = (instr >> 6) & 1;
                const bool w = (instr >> 5) & 1;
                const bool l = (instr >> 4) & 1;
                const bool s = (instr >> 2) & 1;
                const bool h = (instr >> 1) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<
                    &ARM946ES::_ARM_HalfwordAndSignedDataTransfer<p, u, i, w, l, s, h>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0000'0000) {
                const bool ps = (instr >> 6) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_MRS<ps>>;
            } else if constexpr ((instr & 0b1'1011'1111) == 0b1'0010'0000) {
                const bool pd = (instr >> 6) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_MSR<false, pd>>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0001) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_BranchAndExchange>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0110'0001) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CountLeadingZeros>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0011) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_BranchAndLinkExchange>;
            } else if constexpr ((instr & 0b1'1001'1111) == 0b1'0000'0101) {
                const bool dbl = (instr >> 6) & 1;
                const bool sub = (instr >> 5) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_EnhancedDSPAddSub<dbl, sub>>;
            } else if constexpr ((instr & 0b1'1111'1111) == 0b1'0010'0111) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SoftwareBreakpoint>;
            } else if constexpr ((instr & 0b1'1001'1001) == 0b1'0000'1000) {
                const uint8_t op = (instr >> 5) & 0b11;
                const bool y = (instr >> 2) & 1;
                const bool x = (instr >> 1) & 1;
                switch (op) {
                case 0b00: return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SignedMultiplyAccumulate<y, x>>;
                case 0b01:
                    return x ? &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SignedMultiplyWord<y>>
                             : &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SignedMultiplyAccumulateWord<y>>;
                case 0b10: return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SignedMultiplyAccumulateLong<y, x>>;
                case 0b11: return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SignedMultiply<y, x>>;
                }
            } else if constexpr ((instr & 0b1'1001'1001) == 0b1'0000'0001) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
            } else if constexpr ((instr & 0b1001) == 0b1001) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
            } else {
                const uint8_t opcode = (instr >> 5) & 0xF;
                const bool s = (instr >> 4) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_DataProcessing<false, opcode, s>>;
            }
        } else if constexpr (op == 0b001) {
            if constexpr ((instr & 0b1'1011'0000) == 0b1'0010'0000) {
                const bool pd = (instr >> 6) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_MSR<true, pd>>;
            } else if constexpr ((instr & 0b1'1011'0000) == 0b1'0000'0000) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
            } else {
                const uint8_t opcode = (instr >> 5) & 0xF;
                const bool s = (instr >> 4) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_DataProcessing<true, opcode, s>>;
            }
        } else if constexpr (op == 0b010 || op == 0b011) {
            const bool i = op & 1;
            if constexpr (i && (instr & 1)) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_UndefinedInstruction>;
            } else {
                const bool p = (instr >> 8) & 1;
                const bool u = (instr >> 7) & 1;
                const bool b = (instr >> 6) & 1;
                const bool w = (instr >> 5) & 1;
                const bool l = (instr >> 4) & 1;
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SingleDataTransfer<i, p, u, b, w, l>>;
            }
        } else if constexpr (op == 0b100) {
            const bool p = (instr >> 8) & 1;
            const bool u = (instr >> 7) & 1;
            const bool s = (instr >> 6) & 1;
            const bool w = (instr >> 5) & 1;
            const bool l = (instr >> 4) & 1;
            return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_BlockDataTransfer<p, u, s, w, l>>;
        } else if constexpr (op == 0b101) {
            const bool l = (instr >> 8) & 1;
            const bool switchToThumb = specialCond;
            return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_BranchAndBranchWithLink<l, switchToThumb>>;
        } else if constexpr (op == 0b110) {
            // const bool p = (instr >> 8) & 1;
            // const bool u = (instr >> 7) & 1;
            // const bool n = (instr >> 6) & 1;
            // const bool w = (instr >> 5) & 1;
            // const bool l = (instr >> 4) & 1;
            // return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CopDataTransfer<p, u, n, w, l>>;
            return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CopDataTransfer>;
        } else if constexpr (op == 0b111) {
            const bool b24 = (instr >> 8) & 1;
            if constexpr (b24) {
                return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_SoftwareInterrupt>;
            } else {
                const bool b4 = (instr & 1);
                if constexpr (b4) {
                    const uint8_t opcode1 = (instr >> 5) & 0x7;
                    const bool s = (instr >> 4) & 1;
                    const uint16_t opcode2 = (instr >> 1) & 0b111;
                    return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CopRegTransfer<opcode1, s, opcode2>>;
                } else {
                    // const uint8_t opcode1 = (instr >> 4) & 0xF;
                    // const uint8_t opcode2 = (instr >> 1) & 0b111;
                    // return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CopDataOperations<opcode1, opcode2>>;
                    return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_CopDataOperations>;
                }
            }
        } else {
            return &ARM946ES::ARMInstrHandlerWrapper<&ARM946ES::_ARM_Unmapped>;
        }
    }

    template <uint16_t instr>
    static constexpr auto MakeTHUMBInstruction() {
        const uint8_t group = (instr >> 6);
        if constexpr (group == 0b0000 || group == 0b0001) {
            const uint8_t op = (instr >> 5) & 0b11;
            if constexpr (op == 0b11) {
                const bool i = (instr >> 4) & 1;
                const bool subOp = (instr >> 3) & 1;
                const uint8_t rnOrOffset = (instr & 0b111);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_AddSub<i, subOp, rnOrOffset>>;
            } else {
                const uint8_t offset = (instr & 0b11111);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_MoveShiftedRegister<op, offset>>;
            }
        } else if constexpr (group == 0b0010 || group == 0b0011) {
            const uint8_t op = (instr >> 5) & 0b11;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_MovCmpAddSubImmediate<op, rd>>;
        } else if constexpr (group == 0b0100) {
            if constexpr (((instr >> 4) & 0b11) == 0b00) {
                const uint8_t op = (instr & 0b1111);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_ALUOperations<op>>;
            } else if constexpr (((instr >> 4) & 0b11) == 0b01) {
                const uint8_t op = (instr >> 2) & 0b11;
                const bool h1 = (instr >> 1) & 1;
                const bool h2 = (instr & 1);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_HiRegOperations<op, h1, h2>>;
            } else {
                const uint8_t rd = (instr >> 2) & 0b111;
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_PCRelativeLoad<rd>>;
            }
        } else if constexpr (group == 0b0101) {
            if constexpr ((instr >> 3) & 1) {
                const bool h = (instr >> 5) & 1;
                const bool s = (instr >> 4) & 1;
                const uint8_t ro = (instr & 0b111);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LoadStoreSignExtended<h, s, ro>>;
            } else {
                const bool l = (instr >> 5) & 1;
                const bool b = (instr >> 4) & 1;
                const uint8_t ro = (instr & 0b111);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LoadStoreRegOffset<l, b, ro>>;
            }
        } else if constexpr (group == 0b0110 || group == 0b0111) {
            const bool b = (instr >> 6) & 1;
            const bool l = (instr >> 5) & 1;
            const uint16_t offset = (instr & 0b11111) << (b ? 0 : 2);
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LoadStoreImmOffset<b, l, offset>>;
        } else if constexpr (group == 0b1000) {
            const bool l = (instr >> 5) & 1;
            const uint16_t offset = (instr & 0b11111) << 1;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LoadStoreHalfWord<l, offset>>;
        } else if constexpr (group == 0b1001) {
            const bool l = (instr >> 5) & 1;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_SPRelativeLoadStore<l, rd>>;
        } else if constexpr (group == 0b1010) {
            const bool sp = (instr >> 5) & 1;
            const uint8_t rd = (instr >> 2) & 0b111;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LoadAddress<sp, rd>>;
        } else if constexpr (group == 0b1011) {
            if constexpr (((instr >> 2) & 0b1111) == 0b0000) {
                const bool s = (instr >> 1) & 1;
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_AddOffsetToSP<s>>;
            } else if constexpr (((instr >> 2) & 0b1111) == 0b1110) {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_SoftwareBreakpoint>;
            } else if constexpr (((instr >> 2) & 0b0110) == 0b0100) {
                const bool l = (instr >> 5) & 1;
                const bool r = (instr >> 2) & 1;
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_PushPopRegs<l, r>>;
            } else {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_UndefinedInstruction>;
            }
        } else if constexpr (group == 0b1100) {
            const bool l = (instr >> 5) & 1;
            const uint8_t rb = (instr >> 2) & 0b111;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_MultipleLoadStore<l, rb>>;
        } else if constexpr (group == 0b1101) {
            if constexpr (((instr >> 2) & 0b1111) == 0b1111) {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_SoftwareInterrupt>;
            } else if constexpr (((instr >> 2) & 0b1111) == 0b1110) {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_UndefinedInstruction>;
            } else {
                const arm::ConditionFlags cond = static_cast<arm::ConditionFlags>((instr >> 2) & 0xF);
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_ConditionalBranch<cond>>;
            }
        } else if constexpr (group == 0b1110) {
            const bool blx = (instr >> 5) & 1;
            if constexpr (blx) {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LongBranchWithLink<0b01>>;
            } else {
                return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_UnconditionalBranch>;
            }
        } else if constexpr (group == 0b1111) {
            const uint8_t h = (instr >> 5) & 0b11;
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_LongBranchWithLink<h>>;
        } else {
            return &ARM946ES::THUMBInstrHandlerWrapper<&ARM946ES::_THUMB_Unmapped>;
        }
    }

    static constexpr auto MakeARMTable() {
        std::array<std::array<ARMInstructionHandler, 4096>, 2> table{};
        util::constexpr_for<256>([&](auto i) {
            table[0][i * 16 + 0] = MakeARMInstruction<i * 16 + 0, false>();
            table[1][i * 16 + 0] = MakeARMInstruction<i * 16 + 0, true>();
            table[0][i * 16 + 1] = MakeARMInstruction<i * 16 + 1, false>();
            table[1][i * 16 + 1] = MakeARMInstruction<i * 16 + 1, true>();
            table[0][i * 16 + 2] = MakeARMInstruction<i * 16 + 2, false>();
            table[1][i * 16 + 2] = MakeARMInstruction<i * 16 + 2, true>();
            table[0][i * 16 + 3] = MakeARMInstruction<i * 16 + 3, false>();
            table[1][i * 16 + 3] = MakeARMInstruction<i * 16 + 3, true>();
            table[0][i * 16 + 4] = MakeARMInstruction<i * 16 + 4, false>();
            table[1][i * 16 + 4] = MakeARMInstruction<i * 16 + 4, true>();
            table[0][i * 16 + 5] = MakeARMInstruction<i * 16 + 5, false>();
            table[1][i * 16 + 5] = MakeARMInstruction<i * 16 + 5, true>();
            table[0][i * 16 + 6] = MakeARMInstruction<i * 16 + 6, false>();
            table[1][i * 16 + 6] = MakeARMInstruction<i * 16 + 6, true>();
            table[0][i * 16 + 7] = MakeARMInstruction<i * 16 + 7, false>();
            table[1][i * 16 + 7] = MakeARMInstruction<i * 16 + 7, true>();
            table[0][i * 16 + 8] = MakeARMInstruction<i * 16 + 8, false>();
            table[1][i * 16 + 8] = MakeARMInstruction<i * 16 + 8, true>();
            table[0][i * 16 + 9] = MakeARMInstruction<i * 16 + 9, false>();
            table[1][i * 16 + 9] = MakeARMInstruction<i * 16 + 9, true>();
            table[0][i * 16 + 10] = MakeARMInstruction<i * 16 + 10, false>();
            table[1][i * 16 + 10] = MakeARMInstruction<i * 16 + 10, true>();
            table[0][i * 16 + 11] = MakeARMInstruction<i * 16 + 11, false>();
            table[1][i * 16 + 11] = MakeARMInstruction<i * 16 + 11, true>();
            table[0][i * 16 + 12] = MakeARMInstruction<i * 16 + 12, false>();
            table[1][i * 16 + 12] = MakeARMInstruction<i * 16 + 12, true>();
            table[0][i * 16 + 13] = MakeARMInstruction<i * 16 + 13, false>();
            table[1][i * 16 + 13] = MakeARMInstruction<i * 16 + 13, true>();
            table[0][i * 16 + 14] = MakeARMInstruction<i * 16 + 14, false>();
            table[1][i * 16 + 14] = MakeARMInstruction<i * 16 + 14, true>();
            table[0][i * 16 + 15] = MakeARMInstruction<i * 16 + 15, false>();
            table[1][i * 16 + 15] = MakeARMInstruction<i * 16 + 15, true>();
        });
        return table;
    }

    static constexpr auto MakeTHUMBTable() {
        std::array<THUMBInstructionHandler, 1024> table{};
        util::constexpr_for<256>([&](auto i) {
            table[i * 4 + 0] = MakeTHUMBInstruction<i * 4 + 0>();
            table[i * 4 + 1] = MakeTHUMBInstruction<i * 4 + 1>();
            table[i * 4 + 2] = MakeTHUMBInstruction<i * 4 + 2>();
            table[i * 4 + 3] = MakeTHUMBInstruction<i * 4 + 3>();
        });
        return table;
    }

    static constexpr auto s_armTable = MakeARMTable();
    static constexpr auto s_thumbTable = MakeTHUMBTable();
};

} // namespace interp::arm946es
