#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Load coprocessor register
//   mrc[2] <var:dst_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Loads a value from the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2> and stores
// the value in <dst_value>.
struct IRLoadCopRegisterOp : public IROpBase<IROpcodeType::LoadCopRegister> {
    VariableArg dstValue;
    uint8_t cpnum;
    uint8_t opcode1;
    uint8_t crn;
    uint8_t crm;
    uint8_t opcode2;
    bool ext;
    IRLoadCopRegisterOp(VariableArg dstValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2,
                        bool ext)
        : dstValue(dstValue)
        , cpnum(cpnum)
        , opcode1(opcode1)
        , crn(crn)
        , crm(crm)
        , opcode2(opcode2)
        , ext(ext) {}
};

// Store coprocessor register
//   mcr[2] <any:src_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Stores <src_value> into the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2>.
struct IRStoreCopRegisterOp : public IROpBase<IROpcodeType::StoreCopRegister> {
    VarOrImmArg srcValue;
    uint8_t cpnum;
    uint8_t opcode1;
    uint8_t crn;
    uint8_t crm;
    uint8_t opcode2;
    bool ext;

    IRStoreCopRegisterOp(VarOrImmArg srcValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm,
                         uint8_t opcode2, bool ext)
        : srcValue(srcValue)
        , cpnum(cpnum)
        , opcode1(opcode1)
        , crn(crn)
        , crm(crm)
        , opcode2(opcode2)
        , ext(ext) {}
};

// TODO: CDP, CDP2
// TODO: LDC/STC, LDC2/STC2
// TODO: MCRR/MRRC

} // namespace armajitto::ir
