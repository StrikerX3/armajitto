#pragma once

#include <cstdint>

namespace armajitto::x86_64 {

class CPUID {
public:
    enum class Vendor { Intel, AMD, Unknown };

    [[nodiscard]] static Vendor GetVendor() noexcept {
        return s_instance.vendor;
    }

    [[nodiscard]] static bool HasBMI1() noexcept {
        return s_instance.hasBMI1;
    }
    [[nodiscard]] static bool HasBMI2() noexcept {
        return s_instance.hasBMI2;
    }
    [[nodiscard]] static bool HasLZCNT() noexcept {
        return s_instance.hasLZCNT;
    }

    [[nodiscard]] static bool HasFastPDEPAndPEXT() noexcept {
        // Zen1 and Zen2 implement PDEP and PEXT in microcode which has a latency of 18/19 cycles.
        // See: https://www.agner.org/optimize/instruction_tables.pdf

        // Family 17h is AMD Zen, Zen+ and Zen2, all of which have the slow PDEP/PEXT
        return s_instance.hasBMI2 && (s_instance.family != 0x17);
    }

private:
    CPUID() noexcept;

    static CPUID s_instance;

    uint32_t maxLeaf;
    uint32_t maxExtLeaf;

    Vendor vendor = Vendor::Unknown;
    uint8_t family = 0;

    bool hasBMI1 = false;
    bool hasBMI2 = false;
    bool hasLZCNT = false;
};

} // namespace armajitto::x86_64
