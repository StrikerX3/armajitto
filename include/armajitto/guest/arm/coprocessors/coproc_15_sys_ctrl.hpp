#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"
#include "armajitto/util/bit_ops.hpp"

#include <bit>
#include <vector>

namespace armajitto::arm {

class SystemControlCoprocessor : public Coprocessor {
public:
    struct Parameters {
        uint32_t itcmSize;
        uint32_t dtcmSize;
    };

    struct ControlRegister {
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
        } value;

        uint32_t baseVectorAddress;

        void Reset() {
            value.u32 = 0x2078;
            baseVectorAddress = 0xFFFF0000;
        }
    };

    struct ProtectionUnit {
        uint32_t dataCachabilityBits;
        uint32_t codeCachabilityBits;
        uint32_t bufferabilityBits;

        uint32_t dataAccessPermissions;
        uint32_t codeAccessPermissions;

        union Region {
            uint32_t u32;
            struct {
                uint32_t           //
                    enable : 1,    // 0     Protection Region Enable (0=Disable, 1=Enable)
                    size : 5,      // 1-5   Protection Region Size   (2 SHL X) ;min=(X=11)=4KB, max=(X=31)=4GB
                    : 6,           // 6-11  Reserved/zero
                    baseAddr : 20; // 12-31 Protection Region Base address (Addr = Y*4K; must be SIZE-aligned)
            };
        };

        Region regions[8];

        void Reset() {
            dataCachabilityBits = 0;
            codeCachabilityBits = 0;
            bufferabilityBits = 0;
            dataAccessPermissions = 0;
            codeAccessPermissions = 0;
            for (size_t i = 0; i < 8; i++) {
                regions[i].u32 = 0;
            }
        }
    };

    struct TCM {
        std::vector<uint8_t> itcm;
        std::vector<uint8_t> dtcm;

        uint32_t itcmParams;
        uint32_t itcmWriteSize;
        uint32_t itcmReadSize;

        uint32_t dtcmParams;
        uint32_t dtcmBase;
        uint32_t dtcmWriteSize;
        uint32_t dtcmReadSize;

        void Reset() {
            std::fill(itcm.begin(), itcm.end(), 0);
            std::fill(dtcm.begin(), dtcm.end(), 0);

            itcmWriteSize = itcmReadSize = 0;
            itcmParams = 0;

            dtcmBase = 0xFFFFFFFF;
            dtcmWriteSize = dtcmReadSize = 0;
            dtcmParams = 0;
        }

        void Enable(uint32_t itcmSize, uint32_t dtcmSize) {
            itcm.resize(bit::bitceil(itcmSize));
            dtcm.resize(bit::bitceil(dtcmSize));
        }

        void Disable() {
            itcm.clear();
            dtcm.clear();
        }
    };

    SystemControlCoprocessor() = default;

    void Reset() {
        m_ctl.Reset();
        m_pu.Reset();
        m_tcm.Reset();
    }

    // Installs (or reinstalls) the coprocessor with the specified parameters.
    // TCM memory sizes are rounded up to the next power of two not less than the value.
    void Install(const Parameters &params) {
        m_installed = true;
        m_tcm.Enable(params.itcmSize, params.dtcmSize);
        Reset();
    }

    // Uninstalls the coprocessor, freeing up TCM memory.
    void Uninstall() {
        m_installed = false;
        m_tcm.Disable();
        Reset();
    }

    // -------------------------------------------------------------------------
    // Coprocessor interface implementation

    bool IsPresent() final {
        return false;
    }

    bool SupportsExtendedRegTransfers() final {
        return false;
    }

    uint32_t LoadRegister(CopRegister reg) final {
        return 0;
    }

    void StoreRegister(CopRegister reg, uint32_t value) final {}

    bool RegStoreHasSideEffects(CopRegister reg) final {
        return false;
    }

    uint32_t LoadExtRegister(CopRegister reg) final {
        return 0;
    }

    void StoreExtRegister(CopRegister reg, uint32_t value) final {}

    bool ExtRegStoreHasSideEffects(CopRegister reg) final {
        return false;
    }

private:
    bool m_installed = false;

    ControlRegister m_ctl;
    ProtectionUnit m_pu;
    TCM m_tcm;
};

} // namespace armajitto::arm
