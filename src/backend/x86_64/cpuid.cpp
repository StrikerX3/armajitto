#include "armajitto/backend/x86_64/cpuid.hpp"

#ifdef _WIN32
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

namespace armajitto::x86_64 {

CPUID CPUID::s_instance;

CPUID::CPUID() {
    uint32_t eax, ebx, ecx, edx;

    cpuid(0x0000'0001, eax, ebx, ecx, edx);
    family = (eax >> 8) & 0xF;
    if (family == 0xF || family <= 0x1) {
        family += (eax >> 20) & 0xFF;
    }

    cpuid(0x0000'0007, 0, eax, ebx, ecx, edx);
    hasBMI2 = ebx & (1 << 8);

    cpuid(0x8000'0001, eax, ebx, ecx, edx);
    hasLZCNT = ecx & (1 << 5);
}

void CPUID::cpuid(uint32_t leaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx) {
#ifdef _WIN32
    int regs[4];
    __cpuid(regs, leaf);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __cpuid(leaf, &eax, &ebx, &ecx, &edx);
#endif
}

void CPUID::cpuid(uint32_t leaf, uint32_t subleaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx) {
#ifdef _WIN32
    int regs[4];
    __cpuidex(regs, leaf, subleaf);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __cpuidex(leaf, subleaf, &eax, &ebx, &ecx, &edx);
#endif
}

} // namespace armajitto::x86_64
