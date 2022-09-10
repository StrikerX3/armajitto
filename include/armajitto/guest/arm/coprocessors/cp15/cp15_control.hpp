#pragma once

#include <cstdint>

namespace armajitto::arm::cp15 {

struct ControlRegister {
    void Reset();
    void Write(uint32_t value);

    union Value {
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
    };
    static_assert(sizeof(Value) == sizeof(uint32_t), "CP15 control register is must be a 32-bit integer");

    Value value;
    uint32_t baseVectorAddress;
};

} // namespace armajitto::arm::cp15
