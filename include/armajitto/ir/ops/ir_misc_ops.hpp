#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

#include <format>

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

    std::string ToString() const final {
        return std::format("const {}, 0x{:x}", dst.ToString(), value);
    }
};

// Get exception vector base address
//   ld.xvb <var:dst>
//
// Sets <dst> to the exception vector base address, typically 0x00000000 or 0xFFFF0000.
struct IRGetBaseVectorAddressOp : public IROpBase<IROpcodeType::GetBaseVectorAddress> {
    VariableArg dst;

    IRGetBaseVectorAddressOp(VariableArg dst)
        : dst(dst) {}

    std::string ToString() const final {
        return std::format("ld.xvb {}", dst.ToString());
    }
};

} // namespace armajitto::ir
