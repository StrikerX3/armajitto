#pragma once

#include <string>

namespace armajitto::ir {

struct GPRArg {
    uint8_t gpr : 4;
    bool userMode;

    std::string ToString();
};

struct PSRArg {
    bool spsr;

    std::string ToString();
};

struct VariableArg {
    // TODO: variable "name"

    std::string ToString();
};

struct ImmediateArg {
    uint32_t value;

    std::string ToString();
};

struct VarOrImmArg {
    bool immediate;
    union {
        VariableArg var;
        ImmediateArg imm;
    } arg;

    std::string ToString();
};

} // namespace armajitto::ir
