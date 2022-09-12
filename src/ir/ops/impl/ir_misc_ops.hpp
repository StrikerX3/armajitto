#pragma once

#include "../ir_ops_base.hpp"

#include "ir/defs/arguments.hpp"

#include <iomanip>
#include <sstream>
#include <string>

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
        std::ostringstream oss;
        oss << "const " << dst.ToString() << ", #0x" << std::hex << std::uppercase << value;
        return oss.str();
    }
};

// Copy variable
//   copy <var:dst>, <var:value>
//
// Sets <dst> to <value>.
struct IRCopyVarOp : public IROpBase<IROpcodeType::CopyVar> {
    VariableArg dst;
    VariableArg var;

    IRCopyVarOp(VariableArg dst, VariableArg var)
        : dst(dst)
        , var(var) {}

    std::string ToString() const final {
        return std::string("copy ") + dst.ToString() + ", " + var.ToString();
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
        return std::string("ld.xvb ") + dst.ToString();
    }
};

} // namespace armajitto::ir
