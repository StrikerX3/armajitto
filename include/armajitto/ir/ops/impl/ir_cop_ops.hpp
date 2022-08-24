#pragma once

#include "armajitto/guest/arm/cop_register.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Load coprocessor register
//   mrc[2] <var:dst_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Loads a value from the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2> and stores
// the value in <dst_value>.
struct IRLoadCopRegisterOp : public IROpBase<IROpcodeType::LoadCopRegister> {
    VariableArg dstValue;
    uint8_t cpnum;
    arm::CopRegister reg;
    bool ext;
    IRLoadCopRegisterOp(VariableArg dstValue, uint8_t cpnum, arm::CopRegister reg, bool ext)
        : dstValue(dstValue)
        , cpnum(cpnum)
        , reg(reg)
        , ext(ext) {}

    std::string ToString() const final {
        return std::format("mrc{} {}, {}, {}, {}, {}, {}", (ext ? "2" : ""), dstValue.ToString(), cpnum, reg.opcode1,
                           reg.crn, reg.crm, reg.opcode2);
    }
};

// Store coprocessor register
//   mcr[2] <var/imm:src_value>, <int:cpnum>, <int:opcode1>, <int:crn>, <int:crm>, <int:opcode2>
//
// Stores <src_value> into the coprocessor register specified by <cpnum>, <opcode1>, <crn>, <crm> and <opcode2>.
struct IRStoreCopRegisterOp : public IROpBase<IROpcodeType::StoreCopRegister> {
    VarOrImmArg srcValue;
    uint8_t cpnum;
    arm::CopRegister reg;
    bool ext;

    IRStoreCopRegisterOp(VarOrImmArg srcValue, uint8_t cpnum, arm::CopRegister reg, bool ext)
        : srcValue(srcValue)
        , cpnum(cpnum)
        , reg(reg)
        , ext(ext) {}

    std::string ToString() const final {
        return std::format("mcr{} {}, {}, {}, {}, {}, {}", (ext ? "2" : ""), srcValue.ToString(), cpnum, reg.opcode1,
                           reg.crn, reg.crm, reg.opcode2);
    }
};

// TODO: CDP, CDP2
// TODO: LDC/STC, LDC2/STC2
// TODO: MCRR/MRRC

} // namespace armajitto::ir
