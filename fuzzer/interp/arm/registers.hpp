#pragma once

#include <algorithm>
#include <cstdint>

namespace interp::arm {

// Operating mode
enum class Mode : uint32_t {
    User = 0x10,
    FIQ = 0x11,
    IRQ = 0x12,
    Supervisor = 0x13, // aka SWI
    Abort = 0x17,
    Undefined = 0x1B,
    System = 0x1F
};

enum class ExecState { Run, Halt, Stop };

// Index for banked registers
enum Bank {
    Bank_User,
    Bank_FIQ,
    Bank_Supervisor,
    Bank_Abort,
    Bank_IRQ,
    Bank_Undefined,

    Bank_Count
};

// Index for banked registers
enum BankedRegister {
    BReg_R8,
    BReg_R9,
    BReg_R10,
    BReg_R11,
    BReg_R12,
    BReg_R13,
    BReg_R14,

    BReg_Count
};

// Translate Mode into Bank
inline Bank GetBankFromMode(Mode mode) {
    switch (mode) {
    case Mode::User: return Bank_User;
    case Mode::FIQ: return Bank_FIQ;
    case Mode::IRQ: return Bank_IRQ;
    case Mode::Supervisor: return Bank_Supervisor;
    case Mode::Abort: return Bank_Abort;
    case Mode::Undefined: return Bank_Undefined;
    case Mode::System: return Bank_User;
    default: return Bank_User; // TODO: is this correct?
    }
}

// Program Status Register
union PSR {
    uint32_t u32;
    struct {
        Mode mode : 5;  // 0..4   M4-M0 - Mode bits
        uint32_t t : 1; // 5      T - State Bit       (0=ARM, 1=THUMB)
        uint32_t f : 1; // 6      F - FIQ disable     (0=Enable, 1=Disable)
        uint32_t i : 1; // 7      I - IRQ disable     (0=Enable, 1=Disable)
        uint32_t : 19;  // 8..26  Reserved
        uint32_t q : 1; // 27     Q - Sticky Overflow (1=Sticky Overflow, ARMv5TE and up only)
        uint32_t v : 1; // 28     V - Overflow Flag   (0=No Overflow, 1=Overflow)
        uint32_t c : 1; // 29     C - Carry Flag      (0=Borrow/No Carry, 1=Carry/No Borrow)
        uint32_t z : 1; // 30     Z - Zero Flag       (0=Not Zero, 1=Zero)
        uint32_t n : 1; // 31     N - Sign Flag       (0=Not Signed, 1=Signed)
    };
};

// ARM registers
struct Registers {
    // Current set of registers (R0 through R15)
    union {
        uint32_t regs[16];
        struct {
            uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
        };
    };

    // Banked registers R8 through R14
    uint32_t bankregs[Bank_Count][BReg_Count];

    // Program Status Registers
    PSR cpsr;
    PSR spsr[Bank_Count];

    void Reset() {
        std::fill(std::begin(regs), std::end(regs), 0);
        for (size_t i = 0; i < Bank_Count; i++) {
            std::fill(std::begin(bankregs[i]), std::end(bankregs[i]), 0);
            spsr[i].u32 = 0;
        }
        cpsr.u32 = 0;
        cpsr.mode = Mode::Supervisor;
    }

    uint32_t &GPR(size_t index, Mode mode) {
        auto currBank = GetBankFromMode(cpsr.mode);
        auto modeBank = GetBankFromMode(mode);
        if (currBank == modeBank) {
            return regs[index];
        }
        if (modeBank == Bank_FIQ && index >= 8 && index <= 12) {
            return bankregs[Bank_FIQ][index - 8];
        }
        if (modeBank != Bank_User && index >= 13 && index <= 14) {
            return bankregs[modeBank][index - 8];
        }
        return regs[index];
    }

    uint32_t &UserModeGPR(size_t index) {
        auto currBank = GetBankFromMode(cpsr.mode);
        if (currBank == Bank_FIQ && index >= 8 && index <= 12) {
            return bankregs[Bank_User][index - 8];
        }
        if (currBank != Bank_User && (index == 13 || index == 14)) {
            return bankregs[Bank_User][index - 8];
        }
        return regs[index];
    }
};

} // namespace interp::arm
