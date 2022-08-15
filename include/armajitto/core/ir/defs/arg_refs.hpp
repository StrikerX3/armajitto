#pragma once

#include <cstdint>
#include <string>

namespace armajitto::ir {

struct GPRArg {
    uint8_t gpr : 4;
    bool userMode;

    std::string ToString() const;
};

struct PSRArg {
    bool spsr;

    std::string ToString() const;
};

struct VariableArg {
    // TODO: variable "name"

    std::string ToString() const;
};

struct ImmediateArg {
    uint32_t value;

    std::string ToString() const;
};

struct VarOrImmArg {
    bool immediate;
    union {
        VariableArg var;
        ImmediateArg imm;
    } arg;

    std::string ToString() const;
};

} // namespace armajitto::ir
