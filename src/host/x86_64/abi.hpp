#pragma once

#include <xbyak/xbyak.h>

#include <array>

using namespace Xbyak::util;

#ifdef _WIN64
    // https://docs.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170
    #define ARMAJITTO_ABI_WIN64
#else
    // https://gitlab.com/x86-psABIs/x86-64-ABI/-/jobs/artifacts/master/raw/x86-64-ABI/abi.pdf?job=build
    #define ARMAJITTO_ABI_SYSTEMV
#endif

namespace armajitto::abi {

inline constexpr size_t kMaxSpilledRegs = 32;
inline constexpr size_t kRegSpillStackSize = kMaxSpilledRegs * sizeof(uint32_t);

template <size_t alignmentShift, typename T>
inline constexpr T Align(T value) {
    constexpr size_t alignment = static_cast<size_t>(1) << static_cast<size_t>(alignmentShift);
    return (value + alignment - 1) & ~(alignment - 1);
}

#if defined(ARMAJITTO_ABI_WIN64)

inline constexpr std::array<Xbyak::Reg64, 4> kIntArgRegs = {rcx, rdx, r8, r9};
inline constexpr std::array<Xbyak::Reg64, 7> kVolatileRegs = {rax, rcx, rdx, r8, r9, r10, r11};
inline constexpr std::array<Xbyak::Reg64, 8> kNonvolatileRegs = {rbx, rdi, rsi, rbp,
                                                                 r12, r13, r14, r15}; // rsp, unusable

// Windows x64 ABI requires the caller to always allocate space for 4 64-bit registers
inline constexpr size_t kMinStackReserveSize = 4 * sizeof(uint64_t);
inline constexpr size_t kStackReserveSize =
    kRegSpillStackSize < kMinStackReserveSize ? kMinStackReserveSize : Align<4>(kRegSpillStackSize);

#elif defined(ARMAJITTO_ABI_SYSTEMV)

inline constexpr std::array<Xbyak::Reg64, 6> kIntArgRegs = {rdi, rsi, rdx, rcx, r8, r9};
inline constexpr std::array<Xbyak::Reg64, 9> kVolatileRegs = {rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11};
inline constexpr std::array<Xbyak::Reg64, 6> kNonvolatileRegs = {rbx, rbp, r12, r13, r14, r15}; // rsp, unusable

inline constexpr size_t kStackReserveSize = Align<4>(kRegSpillStackSize);

#endif

} // namespace armajitto::abi
