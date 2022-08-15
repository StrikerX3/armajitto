#pragma once

#include "armajitto/core/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Load coprocessor register
//   mrc[2] <var:dst_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Loads a value from the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2> and stores
// the value in <dst_value>.
struct IRLoadCopRegisterOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::LoadCopRegister;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dstValue;
    uint8_t cpnum;
    uint8_t opcode1;
    uint8_t crn;
    uint8_t crm;
    uint8_t opcode2;
    bool ext;
};

// Store coprocessor register
//   mcr[2] <any:src_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Stores <src_value> into the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2>.
struct IRStoreCopRegisterOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::StoreCopRegister;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VarOrImmArg srcValue;
    uint8_t cpnum;
    uint8_t opcode1;
    uint8_t crn;
    uint8_t crm;
    uint8_t opcode2;
    bool ext;
};

// TODO: CDP, CDP2
// TODO: LDC/STC, LDC2/STC2
// TODO: MCRR/MRRC

} // namespace armajitto::ir
