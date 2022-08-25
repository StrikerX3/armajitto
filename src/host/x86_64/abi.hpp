#pragma once

#include <xbyak/xbyak.h>

#include <initializer_list>

using namespace Xbyak::util;

#ifdef _WIN64
    // https://docs.microsoft.com/en-us/cpp/build/x64-software-conventions?view=msvc-170
    #define ARMAJITTO_ABI_WIN64
#else
    // https://gitlab.com/x86-psABIs/x86-64-ABI/-/jobs/artifacts/master/raw/x86-64-ABI/abi.pdf?job=build
    #define ARMAJITTO_ABI_SYSTEMV
#endif

namespace armajitto::abi {

#if defined(ARMAJITTO_ABI_WIN64)

inline constexpr auto kIntArgRegs = {rcx, rdx, r8, r9};
inline constexpr auto kVolatileRegs = {rax, rcx, rdx, r8, r9, r10, r11};
inline constexpr auto kNonvolatileRegs = {rbx, rdi, rsi, rbp, r12, r13, r14, r15}; // rsp, unusable

#elif defined(ARMAJITTO_ABI_SYSTEMV)

inline constexpr auto kIntArgRegs = {rdi, rsi, rdx, rcx, r8, r9};
inline constexpr auto kVolatileRegs = {rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11};
inline constexpr auto kNonvolatileRegs = {rbx, rbp, r12, r13, r14, r15}; // rsp, unusable

#endif

} // namespace armajitto::abi
