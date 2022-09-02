#pragma once

#include "cop_register.hpp"
#include "gpr.hpp"

#include <cstdint>

namespace armajitto::arm {

enum class Condition : uint8_t { EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV };
enum class ShiftType : uint8_t { LSL, LSR, ASR, ROR };

struct RegisterSpecifiedShift {
    ShiftType type;
    bool immediate;
    GPR srcReg;
    union {
        uint8_t imm; // when immediate == true
        GPR reg;     // when immediate == false
    } amount;
};

struct Addressing {
    bool immediate;      // *inverted* I bit
    bool positiveOffset; // U bit
    GPR baseReg;
    union {
        uint16_t immValue;            // when immediate == true
        RegisterSpecifiedShift shift; // when immediate == false
    };
};

namespace instrs {

    // B,BL,BLX (offset)
    struct BranchOffset {
        enum class Type { B, BL, BLX };
        Type type;
        int32_t offset;

        bool IsLink() const {
            return type != Type::B;
        }

        bool IsExchange() const {
            return type == Type::BLX;
        }
    };

    // BX,BLX (register)
    // link  opcode
    //   -   BX
    //   +   BLX
    struct BranchExchangeRegister {
        GPR reg;
        bool link;
    };

    // Thumb BL,BLX suffix
    // blx  opcode
    //  -   BL
    //  +   BLX
    struct ThumbLongBranchSuffix {
        int32_t offset;
        bool blx;
    };

    // AND,EOR,SUB,RSB,ADD,ADC,SBC,RSC,TST,TEQ,CMP,CMN,ORR,MOV,BIC,MVN
    struct DataProcessing {
        enum class Opcode : uint8_t { AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN };

        Opcode opcode;
        bool immediate;
        bool setFlags;
        GPR dstReg; // Rd
        GPR lhsReg; // Rn
        union {
            uint32_t imm;                 // (when immediate == true)
            RegisterSpecifiedShift shift; // (when immediate == false)
        } rhs;
        bool thumbPCAdjust = false; // AND value with ~3 if lhsReg == PC (for Thumb Load Address instruction)
    };

    // CLZ
    struct CountLeadingZeros {
        GPR dstReg; // Rd
        GPR argReg; // Rm
    };

    // QADD,QSUB,QDADD,QDSUB
    // sub dbl  opcode
    //  -   -   QADD
    //  -   +   QDADD
    //  +   -   QSUB
    //  +   +   QDSUB
    struct SaturatingAddSub {
        GPR dstReg; // Rd
        GPR lhsReg; // Rm
        GPR rhsReg; // Rn
        bool sub;
        bool dbl;
    };

    // MUL,MLA
    // accumulate  opcode
    //      -      MUL
    //      +      MLA
    struct MultiplyAccumulate {
        GPR dstReg; // Rd
        GPR lhsReg; // Rm
        GPR rhsReg; // Rs
        GPR accReg; // Rn (when accumulate == true)
        bool accumulate;
        bool setFlags; // S bit
    };

    // SMULL,UMULL,SMLAL,UMLAL
    // signedMul accumulate  opcode
    //     -          -      UMULL
    //     -          +      UMLAL
    //     +          -      SMULL
    //     +          +      SMLAL
    struct MultiplyAccumulateLong {
        GPR dstAccLoReg; // RdHi (also accumulator when accumulate == true)
        GPR dstAccHiReg; // RdLo (also accumulator when accumulate == true)
        GPR lhsReg;      // Rm
        GPR rhsReg;      // Rs
        bool signedMul;
        bool accumulate;
        bool setFlags; // S bit
    };

    // SMUL<x><y>,SMLA<x><y>
    // accumulate  opcode
    //      -      SMUL<x><y>
    //      +      SMLA<x><y>
    struct SignedMultiplyAccumulate {
        GPR dstReg; // Rd
        GPR lhsReg; // Rm
        GPR rhsReg; // Rs
        GPR accReg; // Rn (when accumulate == true)
        bool x;
        bool y;
        bool accumulate;
    };

    // SMULW<y>,SMLAW<y>
    // accumulate  opcode
    //      -      SMULW<y>
    //      +      SMLAW<y>
    struct SignedMultiplyAccumulateWord {
        GPR dstReg; // Rd
        GPR lhsReg; // Rm
        GPR rhsReg; // Rs
        GPR accReg; // Rn (when accumulate == true)
        bool y;
        bool accumulate;
    };

    // SMLAL<x><y>
    struct SignedMultiplyAccumulateLong {
        GPR dstAccLoReg; // RdLo
        GPR dstAccHiReg; // RdHi
        GPR lhsReg;      // Rm
        GPR rhsReg;      // Rs
        bool x;
        bool y;
    };

    // MRS
    struct PSRRead {
        bool spsr;
        GPR dstReg; // Rd
    };

    // MSR
    struct PSRWrite {
        bool immediate;
        bool spsr;
        bool f;
        bool s;
        bool x;
        bool c;
        union {
            uint32_t imm; // (when immediate == true)
            GPR reg;      // Rm (when immediate == false)
        } value;
    };

    // LDR,STR,LDRB,STRB
    // byte load  opcode
    //   -    -   STR
    //   -    +   LDR
    //   +    -   STRB
    //   +    +   LDRB
    struct SingleDataTransfer {
        bool preindexed; // P bit
        bool byte;       // B bit
        bool writeback;  // W bit
        bool load;       // L bit
        GPR reg;         // Rd
        Addressing address;
        bool thumbPCAdjust = false; // AND value with ~3 if reg == PC (for Thumb PC-Relative Load instruction)
    };

    // LDRH,STRH,LDRSH,LDRSB,LDRD,STRD
    // load sign half  opcode
    //   -    -    +   STRH
    //   -    +    -   LDRD (ARMv5TE only -- Undefined otherwise)
    //   -    +    +   STRD (ARMv5TE only -- Undefined otherwise)
    //   +    -    +   LDRH
    //   +    +    -   LDRSB
    //   +    +    +   LDRSH
    //  any   -    -   (SWP/SWPB -- SingleDataSwap)
    struct HalfwordAndSignedTransfer {
        bool preindexed;     // P bit
        bool positiveOffset; // U bit
        bool immediate;      // I bit
        bool writeback;      // W bit
        bool load;           // L bit
        bool sign;           // S bit
        bool half;           // H bit
        GPR reg;             // Rd
        GPR baseReg;         // Rn
        union {
            uint16_t imm; // (when immediate == true)
            GPR reg;      // Rm (when immediate == false)
        } offset;
    };

    // LDM,STM
    // load  opcode
    //   -   STM
    //   +   LDM
    struct BlockTransfer {
        bool preindexed;            // P bit
        bool positiveOffset;        // U bit
        bool userModeOrPSRTransfer; // S bit
        bool writeback;             // W bit
        bool load;                  // L bit
        GPR baseReg;                // Rn
        uint16_t regList;
    };

    // SWP,SWPB
    // byte  opcode
    //   -   SWP
    //   +   SWPB
    struct SingleDataSwap {
        bool byte;      // B bit
        GPR dstReg;     // Rd
        GPR valueReg;   // Rm
        GPR addressReg; // Rn
    };

    // SWI
    struct SoftwareInterrupt {
        uint32_t comment;
    };

    // BKPT
    struct SoftwareBreakpoint {};

    // PLD
    struct Preload {
        Addressing address;
    };

    // CDP,CDP2
    // ext  opcode
    //  -   CDP
    //  +   CDP2
    struct CopDataOperations {
        uint8_t opcode1;
        uint8_t crn;
        uint8_t crd;
        uint8_t cpnum;
        uint8_t opcode2;
        uint8_t crm;
        bool ext;
    };

    // STC,STC2,LDC,LDC2
    // load ext  opcode
    //   -   -   STC
    //   -   +   STC2
    //   +   -   LDC
    //   +   +   LDC2
    struct CopDataTransfer {
        bool preindexed;     // P bit
        bool positiveOffset; // U bit
        bool n;              // N bit
        bool writeback;      // W bit
        bool load;           // L bit
        GPR rn;
        uint8_t crd;
        uint8_t cpnum;
        uint8_t offset;
        bool ext;
    };

    // MCR,MCR2,MRC,MRC2
    // load ext  opcode
    //   -   -   MCR
    //   -   +   MCR2
    //   +   -   MRC
    //   +   +   MRC2
    struct CopRegTransfer {
        bool load;
        GPR rd;
        uint8_t cpnum;
        CopRegister reg;
        bool ext; // false = MCR/MRC; true = MCR2/MRC2
    };

    // MCRR,MRRC
    // load  opcode
    //   -   MCRR
    //   +   MRRC
    struct CopDualRegTransfer {
        bool load;
        GPR rn;
        GPR rd;
        uint8_t cpnum;
        uint8_t opcode;
        uint8_t crm;
    };

    // UDF and other undefined instructions
    struct Undefined {};

} // namespace instrs

} // namespace armajitto::arm
