#pragma once

#ifdef _WIN32
    #define NOMINMAX
#endif
#include <xbyak/xbyak.h>

#include <array>

#ifdef _WIN64
    // https://docs.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170
    #define ARMAJITTO_ABI_WIN64
#else
    // https://gitlab.com/x86-psABIs/x86-64-ABI/-/jobs/artifacts/master/raw/x86-64-ABI/abi.pdf?job=build
    #define ARMAJITTO_ABI_SYSTEMV
#endif

// ---------------------------------------------------------------------------------------------------------------------

using namespace Xbyak::util;

namespace armajitto::abi {

// Returns the smallest integer greater than or equal to <value> that has zeros in the least significant
// <alignmentShift> bits. In other words, aligns the value to the specified alignment.
template <size_t alignmentShift, typename T>
inline constexpr T Align(T value) {
    constexpr size_t alignment = static_cast<size_t>(1) << static_cast<size_t>(alignmentShift);
    return (value + alignment - 1) & ~(alignment - 1);
}

// ---------------------------------------------------------------------------------------------------------------------
// Register spill area definitions

// Maximum number of spilled registers -- determines the stack reserve size
inline constexpr size_t kMaxSpilledRegs = 32;

// Size of register spill area in bytes
inline constexpr size_t kRegSpillStackSize = kMaxSpilledRegs * sizeof(uint32_t);

// Statically allocaated registers
inline constexpr auto kHostFlagsReg = eax; // eax = host flags (ah = NZC, al = V)
inline constexpr auto kARMStateReg = rbx;  // rbx = pointer to ARM state struct
inline constexpr auto kShiftCounter = rcx; // rcx = shift counter (for use in shift operations)

// ---------------------------------------------------------------------------------------------------------------------
// ABI specifications for each supported system.
//
// Function calls, arguments and return value
// - kIntArgRegs: integer registers passed as arguments to functions, from first to last
// - kVolatileRegs: caller-saved registers
// - kNonvolatileRegs: callee-saved registers
// - kRetValReg: integer return value register
//
// Stack
// - kStackAlignmentShift: number of least significant zero bits for the stack to be aligned
// - kStackGrowsDownward: true if the stack grows downward (i.e. push/call decreases RSP, pop/ret increases)
// - kMinStackReserveSize: minimum number of bytes required to be reserved in the stack for function calls
// - kStackReserveSize: number of bytes to reserve for register spilling, including the minimum stack reserve size

#if defined(ARMAJITTO_ABI_WIN64)

inline constexpr std::array<Xbyak::Reg64, 4> kIntArgRegs = {rcx, rdx, r8, r9};
inline constexpr std::array<Xbyak::Reg64, 7> kVolatileRegs = {rax, rcx, rdx, r8, r9, r10, r11};
inline constexpr std::array<Xbyak::Reg64, 8> kNonvolatileRegs = {rbx, rdi, rsi, rbp, r12, r13, r14, r15};
// rsp is also volatible, but unusable

inline constexpr Xbyak::Reg64 kIntReturnValueReg = rax;

inline constexpr uint64_t kStackAlignmentShift = 4ull;
inline constexpr bool kStackGrowsDownward = true;

// Windows x64 ABI requires the caller to always allocate space for 4 64-bit registers
inline constexpr size_t kMinStackReserveSize = 4 * sizeof(uint64_t);

// ---------------------------------------------------------------------------------------------------------------------

#elif defined(ARMAJITTO_ABI_SYSTEMV)

inline constexpr std::array<Xbyak::Reg64, 6> kIntArgRegs = {rdi, rsi, rdx, rcx, r8, r9};
inline constexpr std::array<Xbyak::Reg64, 9> kVolatileRegs = {rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11};
inline constexpr std::array<Xbyak::Reg64, 6> kNonvolatileRegs = {rbx, rbp, r12, r13, r14, r15};
// rsp is also volatible, but unusable

inline constexpr Xbyak::Reg64 kIntReturnValueReg = rax;

inline constexpr uint64_t kStackAlignmentShift = 4ull;
inline constexpr bool kStackGrowsDownward = true;

inline constexpr size_t kMinStackReserveSize = 0;

#endif

// ---------------------------------------------------------------------------------------------------------------------

inline constexpr size_t kStackReserveSize = (kRegSpillStackSize < kMinStackReserveSize)
                                                ? kMinStackReserveSize
                                                : Align<kStackAlignmentShift>(kRegSpillStackSize);

} // namespace armajitto::abi
