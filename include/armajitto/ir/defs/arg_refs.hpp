#pragma once

#include "armajitto/ir/defs/variable.hpp"

#include <cstdint>

namespace armajitto::ir {

struct GPRArg {
    uint8_t gpr : 4;
    bool userMode;
};

struct PSRArg {
    bool spsr;
    uint8_t mode; // when spsr == true
};

struct VariableArg {
    uint32_t varIndex;

    VariableArg &operator=(const Variable &var) {
        varIndex = var.index;
        return *this;
    }
};

struct ImmediateArg {
    uint32_t value;

    ImmediateArg &operator=(uint32_t imm) {
        value = imm;
        return *this;
    }
};

struct VarOrImmArg {
    bool immediate;
    union {
        VariableArg var;
        ImmediateArg imm;
    } arg;

    VarOrImmArg &operator=(const Variable &var) {
        immediate = false;
        arg.var = var;
        return *this;
    }

    VarOrImmArg &operator=(uint32_t imm) {
        immediate = true;
        arg.imm = imm;
        return *this;
    }
};

} // namespace armajitto::ir
