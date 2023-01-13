#include "cpuid.hpp"

#ifdef _WIN32
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

namespace armajitto::x86_64 {

inline void cpuid(uint32_t leaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx) noexcept {
#ifdef _WIN32
    int regs[4];
    __cpuid(regs, leaf);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __cpuid(leaf, eax, ebx, ecx, edx);
#endif
}

inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx,
                  uint32_t &edx) noexcept {
#ifdef _WIN32
    int regs[4];
    __cpuidex(regs, leaf, subleaf);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
#endif
}

CPUID CPUID::s_instance;

CPUID::CPUID() noexcept {
    uint32_t eax, ebx, ecx, edx;

    // https://sandpile.org/x86/cpuid.htm

    // Get maximum standard level and vendor string
    cpuid(0x0000'0000, eax, ebx, ecx, edx);
    maxLeaf = eax;

    if (ebx == 0x756E6547 && edx == 0x49656E69 && ecx == 0x6C65746E) {
        vendor = Vendor::Intel; // GenuineIntel
    } else if (ebx == 0x68747541 && edx == 0x69746E65 && ecx == 0x444D4163) {
        vendor = Vendor::AMD; // AuthenticAMD
    } else {
        vendor = Vendor::Unknown;
    }

    // Get family
    if (maxLeaf >= 0x0000'0001) {
        cpuid(0x0000'0001, eax, ebx, ecx, edx);
        family = (eax >> 8) & 0xF;
        if (family == 0xF || family <= 0x1) {
            family += (eax >> 20) & 0xFF;
        }
    }

    // Detect BMI2 instruction set
    if (maxLeaf >= 0x0000'0007) {
        cpuid(0x0000'0007, 0, eax, ebx, ecx, edx);
        hasBMI2 = ebx & (1 << 8);
    }

    // Get maximum extended level
    cpuid(0x8000'0000, eax, ebx, ecx, edx);
    maxExtLeaf = eax;

    // Detect LZCNT instruction set
    if (maxExtLeaf >= 0x8000'0001) {
        cpuid(0x8000'0001, eax, ebx, ecx, edx);
        hasLZCNT = ecx & (1 << 5);
    }
}

} // namespace armajitto::x86_64
