#pragma once

#include <cstdint>

namespace armajitto::arm {

enum class Condition : uint8_t { EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV };
enum class ShiftType : uint8_t { LSL, LSR, ASR, ROR };

struct RegisterSpecifiedShift {
    ShiftType type;
    bool immediate;
    uint8_t srcReg;
    union {
        uint8_t imm; // when immediate == true
        uint8_t reg; // when immediate == false
    } amount;
};

struct AddressingOffset {
    bool immediate;      // *inverted* I bit
    bool positiveOffset; // U bit
    uint8_t baseReg;
    union {
        uint16_t immValue;            // when immediate == true
        RegisterSpecifiedShift shift; // when immediate == false
    };
};

namespace instrs {

    // B,BL
    // link  opcode
    //   -   B
    //   +   BL
    struct Branch {
        int32_t offset;
        bool link;
        bool switchToThumb;
    };

    // BX,BLX
    // link  opcode
    //   -   BX
    //   +   BLX
    struct BranchAndExchange {
        uint8_t reg;
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
        uint8_t dstReg; // Rd
        uint8_t lhsReg; // Rn
        union {
            uint32_t imm;                 // (when immediate == true)
            RegisterSpecifiedShift shift; // (when immediate == false)
        } rhs;
    };

    // CLZ
    struct CountLeadingZeros {
        uint8_t dstReg; // Rd
        uint8_t argReg; // Rm
    };

    // QADD,QSUB,QDADD,QDSUB
    // sub dbl  opcode
    //  -   -   QADD
    //  -   +   QDADD
    //  +   -   QSUB
    //  +   +   QDSUB
    struct SaturatingAddSub {
        uint8_t dstReg; // Rd
        uint8_t lhsReg; // Rm
        uint8_t rhsReg; // Rn
        bool sub;
        bool dbl;
    };

    // MUL,MLA
    // accumulate  opcode
    //      -      MUL
    //      +      MLA
    struct MultiplyAccumulate {
        uint8_t dstReg; // Rd
        uint8_t lhsReg; // Rm
        uint8_t rhsReg; // Rs
        uint8_t accReg; // Rn (when accumulate == true)
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
        uint8_t dstAccLoReg; // RdHi (also accumulator when accumulate == true)
        uint8_t dstAccHiReg; // RdLo (also accumulator when accumulate == true)
        uint8_t lhsReg;      // Rm
        uint8_t rhsReg;      // Rs
        bool signedMul;
        bool accumulate;
        bool setFlags; // S bit
    };

    // SMUL<x><y>,SMLA<x><y>
    // accumulate  opcode
    //      -      SMUL<x><y>
    //      +      SMLA<x><y>
    struct SignedMultiplyAccumulate {
        uint8_t dstReg; // Rd
        uint8_t lhsReg; // Rm
        uint8_t rhsReg; // Rs
        uint8_t accReg; // Rn (when accumulate == true)
        bool x;
        bool y;
        bool accumulate;
    };

    // SMULW<y>,SMLAW<y>
    // accumulate  opcode
    //      -      SMULW<y>
    //      +      SMLAW<y>
    struct SignedMultiplyAccumulateWord {
        uint8_t dstReg; // Rd
        uint8_t lhsReg; // Rm
        uint8_t rhsReg; // Rs
        uint8_t accReg; // Rn (when accumulate == true)
        bool y;
        bool accumulate;
    };

    // SMLAL<x><y>
    struct SignedMultiplyAccumulateLong {
        uint8_t dstAccLoReg; // RdLo
        uint8_t dstAccHiReg; // RdHi
        uint8_t lhsReg;      // Rm
        uint8_t rhsReg;      // Rs
        bool x;
        bool y;
    };

    // MRS
    struct PSRRead {
        bool spsr;
        uint8_t dstReg; // Rd
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
            uint8_t reg;  // Rm (when immediate == false)
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
        uint8_t dstReg;  // Rd
        AddressingOffset offset;
    };

    // LDRH,STRH,LDRSH,LDRSB,LDRD,STRD
    // load sign half  opcode
    //  any   -    -   SWP/SWPB
    //   -    -    +   STRH
    //   -    +    -   LDRD (or UDF if bit 12 is set)  (ARMv5TE only)
    //   -    +    +   STRD (or UDF if bit 12 is set)  (ARMv5TE only)
    //   +    -    +   LDRH
    //   +    +    -   LDRSB
    //   +    +    +   LDRSH
    struct HalfwordAndSignedTransfer {
        bool preindexed;     // P bit
        bool positiveOffset; // U bit
        bool immediate;      // I bit
        bool writeback;      // W bit
        bool load;           // L bit
        bool sign;           // S bit
        bool half;           // H bit
        uint8_t dstReg;      // Rd
        uint8_t baseReg;     // Rn
        union {
            uint16_t imm; // (when immediate == true)
            uint8_t reg;  // Rm (when immediate == false)
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
        uint8_t baseReg;            // Rn
        uint16_t regList;
    };

    // SWP,SWPB
    // byte  opcode
    //   -   SWP
    //   +   SWPB
    struct SingleDataSwap {
        bool byte;          // B bit
        uint8_t dstReg;     // Rd
        uint8_t valueReg;   // Rm
        uint8_t addressReg; // Rn
    };

    // SWI
    struct SoftwareInterrupt {
        uint32_t comment;
    };

    // BKPT
    struct SoftwareBreakpoint {};

    // PLD
    struct Preload {
        AddressingOffset offset;
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
        uint8_t rn;
        uint8_t crd;
        uint8_t cpnum;
        uint8_t offset;
        bool ext;
    };

    // MCR,MCR2,MRC,MRC2
    // store ext  opcode
    //   -    -   MCR
    //   -    +   MCR2
    //   +    -   MRC
    //   +    +   MRC2
    struct CopRegTransfer {
        bool store;
        uint8_t opcode1;
        uint16_t crn;
        uint8_t rd;
        uint8_t cpnum;
        uint16_t opcode2;
        uint16_t crm;
        bool ext; // false = MCR/MRC; true = MCR2/MRC2
    };

    // MCRR,MRRC
    // store  opcode
    //   -    MCRR
    //   +    MRRC
    struct CopDualRegTransfer {
        bool store;
        uint8_t rn;
        uint8_t rd;
        uint8_t cpnum;
        uint8_t opcode;
        uint8_t crm;
    };

    // UDF and other undefined instructions
    struct Undefined {};

} // namespace instrs

} // namespace armajitto::arm
