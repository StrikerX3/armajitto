#pragma once

#include "armajitto/defs/arm/gpr.hpp"
#include "armajitto/defs/arm/mode.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <cstdint>
#include <utility>

namespace armajitto::ir {

struct GPRArg {
    GPR gpr;
    bool userMode;

    GPRArg(GPR gpr)
        : gpr(gpr)
        , userMode(false) {}

    GPRArg(GPR gpr, bool userMode)
        : gpr(gpr)
        , userMode(userMode) {}
};

struct VariableArg {
    Variable var;

    VariableArg() = default;

    VariableArg(const Variable &var) {
        operator=(var);
    }

    VariableArg &operator=(const Variable &var) {
        this->var = var;
        return *this;
    }
};

struct ImmediateArg {
    uint32_t value;

    ImmediateArg()
        : value(0) {}

    ImmediateArg(uint32_t imm) {
        operator=(imm);
    }

    ImmediateArg &operator=(uint32_t imm) {
        value = imm;
        return *this;
    }
};

struct VarOrImmArg {
    bool immediate;
    VariableArg var;  // when immediate == false
    ImmediateArg imm; // when immediate == true

    VarOrImmArg() {
        operator=(0);
    }

    VarOrImmArg(Variable &var) {
        operator=(var);
    }

    VarOrImmArg(uint32_t imm) {
        operator=(imm);
    }

    VarOrImmArg &operator=(const Variable &var) {
        immediate = false;
        this->var = var;
        return *this;
    }

    VarOrImmArg &operator=(uint32_t imm) {
        immediate = true;
        this->imm = imm;
        return *this;
    }
};

} // namespace armajitto::ir
