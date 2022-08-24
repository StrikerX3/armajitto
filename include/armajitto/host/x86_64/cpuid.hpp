#pragma once

#include <cstdint>

namespace armajitto::x86_64 {

class CPUID {
public:
    enum class Vendor { Intel, AMD, Unknown };

    static CPUID &Instance() noexcept {
        return s_instance;
    }

    [[nodiscard]] Vendor GetVendor() const noexcept {
        return vendor;
    }

    [[nodiscard]] bool HasBMI2() const noexcept {
        return hasBMI2;
    }
    [[nodiscard]] bool HasLZCNT() const noexcept {
        return hasLZCNT;
    }

    [[nodiscard]] bool HasFastPDEPAndPEXT() const noexcept {
        // Zen1 and Zen2 implement PDEP and PEXT in microcode which has a latency of 18/19 cycles.
        // See: https://www.agner.org/optimize/instruction_tables.pdf

        // Family 17h is AMD Zen, Zen+ and Zen2, all of which have the slow PDEP/PEXT
        return hasBMI2 && (family != 0x17);
    }

private:
    CPUID() noexcept;

    static CPUID s_instance;

    uint32_t maxLeaf;
    uint32_t maxExtLeaf;

    Vendor vendor = Vendor::Unknown;
    uint8_t family = 0;

    bool hasBMI2 = false;
    bool hasLZCNT = false;
};

} // namespace armajitto::x86_64
