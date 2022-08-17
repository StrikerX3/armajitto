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

} // namespace armajitto::ir
