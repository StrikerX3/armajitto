#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Define constant
//   const <var:dst>, <imm:value>
//
// Sets <dst> to <value>.
struct IRConstantOp : public IROpBase<IROpcodeType::Constant> {
    VariableArg dst;
    uint32_t value;

    IRConstantOp(VariableArg dst, uint32_t value)
        : dst(dst)
        , value(value) {}
};

// Get base exception vector address
//   ld.vecbase <var:dst>
//
// Sets <dst> to the base exception vector address, typically 0x00000000 or 0xFFFF0000.
struct IRGetBaseVectorAddressOp : public IROpBase<IROpcodeType::GetBaseVectorAddress> {
    VariableArg dst;

    IRGetBaseVectorAddressOp(VariableArg dst)
        : dst(dst) {}
};

} // namespace armajitto::ir
