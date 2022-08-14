#pragma once

#include <cstdint>

namespace armajitto {

class CPUID {
public:
    CPUID();

    [[nodiscard]] bool HasBMI2() const { return hasBMI2; }
    [[nodiscard]] bool HasLZCNT() const { return hasLZCNT; }

    [[nodiscard]] bool HasFastPDEPAndPEXT() const {
        if (!hasBMI2) {
            return false;
        }

        // Zen1 and Zen2 implement PDEP and PEXT in microcode which has a latency of 18/19 cycles.
        // See: https://www.agner.org/optimize/instruction_tables.pdf

        // Family 17h is AMD Zen, Zen+ and Zen2, all of which have the slow PDEP/PEXT
        return (family != 0x17);
    }

private:
    uint8_t family;

    bool hasBMI2;
    bool hasLZCNT;

    static void cpuid(uint32_t leaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx);
    static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t &eax, uint32_t &ebx, uint32_t &ecx, uint32_t &edx);
};

} // namespace armajitto
