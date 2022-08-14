#pragma once

#include "decoder_common.hpp"

namespace armajitto::arm::decoder::instrs {

// B,BL
// link  opcode
//   -   B
//   +   BL
struct Branch {
    Condition cond;
    int32_t offset;
    bool link;
    bool switchToThumb;
};

// BX,BLX
// link  opcode
//   -   BX
//   +   BLX
struct BranchAndExchange {
    Condition cond;
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

    Condition cond;
    Opcode opcode;
    bool immediate;
    bool setFlags;
    uint8_t dstReg;
    uint8_t lhsReg;
    union {
        uint32_t imm;                 // when immediate == true
        RegisterSpecifiedShift shift; // when immediate == false
    } rhs;
};

// CLZ
struct CountLeadingZeros {
    Condition cond;
    uint8_t dstReg;
    uint8_t argReg;
};

// QADD,QSUB,QDADD,QDSUB
// sub dbl  opcode
//  -   -   QADD
//  -   +   QDADD
//  +   -   QSUB
//  +   +   QDSUB
struct SaturatingAddSub {
    Condition cond;
    uint8_t dstReg;
    uint8_t lhsReg;
    uint8_t rhsReg;
    bool sub;
    bool dbl;
};

// MUL,MLA
// accumulate  opcode
//      -      MUL
//      +      MLA
struct MultiplyAccumulate {
    Condition cond;
    uint8_t dstReg;
    uint8_t lhsReg;
    uint8_t rhsReg;
    uint8_t accReg; // when accumulate == true
    bool accumulate;
    bool setFlags;
};

// SMULL,UMULL,SMLAL,UMLAL
// signedMul accumulate  opcode
//     -          -      UMULL
//     -          +      UMLAL
//     +          -      SMULL
//     +          +      SMLAL
struct MultiplyAccumulateLong {
    Condition cond;
    uint8_t dstAccHiReg; // also accumulator when accumulate == true
    uint8_t dstAccLoReg; // also accumulator when accumulate == true
    uint8_t lhsReg;
    uint8_t rhsReg;
    bool signedMul;
    bool accumulate;
    bool setFlags;
};

// SMUL<x><y>,SMLA<x><y>
// accumulate  opcode
//      -      SMUL<x><y>
//      +      SMLA<x><y>
struct SignedMultiplyAccumulate {
    Condition cond;
    uint8_t dstReg;
    uint8_t lhsReg;
    uint8_t rhsReg;
    uint8_t accReg; // when accumulate == true
    bool x;
    bool y;
    bool accumulate;
};

// SMULW<y>,SMLAW<y>
// accumulate  opcode
//      -      SMULW<y>
//      +      SMLAW<y>
struct SignedMultiplyAccumulateWord {
    Condition cond;
    uint8_t dstReg;
    uint8_t lhsReg;
    uint8_t rhsReg;
    uint8_t accReg; // when accumulate == true
    bool y;
    bool accumulate;
};

// SMLAL<x><y>
struct SignedMultiplyAccumulateLong {
    Condition cond;
    uint8_t dstAccHiReg;
    uint8_t dstAccLoReg;
    uint8_t lhsReg;
    uint8_t rhsReg;
    bool x;
    bool y;
};

// MRS
struct PSRRead {
    Condition cond;
    bool spsr;
    uint8_t dstReg;
};

// MSR
struct PSRWrite {
    Condition cond;
    bool immediate;
    bool spsr;
    bool f;
    bool s;
    bool x;
    bool c;
    union {
        uint32_t imm; // when immediate == true
        uint8_t reg;  // when immediate == false
    } value;
};

// LDR,STR,LDRB,STRB
// byte load  opcode
//   -    -   STR
//   -    +   LDR
//   +    -   STRB
//   +    +   LDRB
struct SingleDataTransfer {
    Condition cond;
    bool preindexed; // P bit
    bool byte;       // B bit
    bool writeback;  // W bit
    bool load;       // L bit
    uint8_t dstReg;
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
    Condition cond;
    bool preindexed;     // P bit
    bool positiveOffset; // U bit
    bool immediate;      // I bit
    bool writeback;      // W bit
    bool load;           // L bit
    bool sign;           // S bit
    bool half;           // H bit
    uint8_t dstReg;
    uint8_t baseReg;
    union {
        uint16_t imm; // when immediate == true
        uint8_t reg;  // when immediate = false
    } offset;
};

// LDM,STM
// load  opcode
//   -   STM
//   +   LDM
struct BlockTransfer {
    Condition cond;
    bool preindexed;     // P bit
    bool positiveOffset; // U bit
    bool userMode;       // S bit
    bool writeback;      // W bit
    bool load;           // L bit
    uint8_t baseReg;
    uint16_t regList;
};

// SWP,SWPB
// byte  opcode
//   -   SWP
//   +   SWPB
struct SingleDataSwap {
    Condition cond;
    bool byte; // B bit
    uint8_t dstReg;
    uint8_t addressReg1;
    uint8_t addressReg2;
};

// SWI
struct SoftwareInterrupt {
    Condition cond;
};

// BKPT
struct SoftwareBreakpoint {
    Condition cond;
};

// PLD
struct Preload {
    AddressingOffset offset;
};

// CDP,CDP2
// ext  opcode
//  -   CDP
//  +   CDP2
struct CopDataOperations {
    Condition cond;
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
    Condition cond;
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
    Condition cond;
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
    Condition cond;
    bool store;
    uint8_t rn;
    uint8_t rd;
    uint8_t cpnum;
    uint8_t opcode;
    uint8_t crm;
};

// UDF and other undefined instructions
struct Undefined {
    Condition cond;
};

} // namespace armajitto::arm::decoder::instrs
