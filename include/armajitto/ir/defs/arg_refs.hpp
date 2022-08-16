#pragma once

#include "armajitto/defs/arm/mode.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <cstdint>
#include <utility>

namespace armajitto::ir {

struct GPRArg {
    uint8_t gpr : 4;
    bool userMode;

    GPRArg()
        : gpr(0) {}

    GPRArg(const GPRArg &) = default;
    GPRArg(GPRArg &&) = default;

    GPRArg(uint8_t gpr) {
        operator=(gpr);
    }

    GPRArg &operator=(const GPRArg &) = default;
    GPRArg &operator=(GPRArg &&) = default;

    GPRArg &operator=(uint8_t gpr) {
        this->gpr = gpr;
        return *this;
    }
};

struct PSRArg {
    bool spsr;
    arm::Mode mode; // when spsr == true
};

struct VariableArg {
    size_t varIndex;

    VariableArg()
        : varIndex(Variable::kInvalidIndex) {}

    VariableArg(const VariableArg &) = default;
    VariableArg(VariableArg &&) = default;

    VariableArg(const Variable &var) {
        operator=(var);
    }

    VariableArg &operator=(const VariableArg &) = default;
    VariableArg &operator=(VariableArg &&) = default;

    VariableArg &operator=(const Variable &var) {
        varIndex = var.index;
        return *this;
    }
};

struct ImmediateArg {
    uint32_t value;

    ImmediateArg()
        : value(0) {}

    ImmediateArg(const ImmediateArg &) = default;
    ImmediateArg(ImmediateArg &&) = default;

    ImmediateArg(uint32_t imm) {
        operator=(imm);
    }

    ImmediateArg &operator=(const ImmediateArg &) = default;
    ImmediateArg &operator=(ImmediateArg &&) = default;

    ImmediateArg &operator=(uint32_t imm) {
        value = imm;
        return *this;
    }
};

struct VarOrImmArg {
    bool immediate;
    VariableArg var;
    ImmediateArg imm;

    VarOrImmArg() {
        operator=(0);
    }

    VarOrImmArg(const VarOrImmArg &) = default;
    VarOrImmArg(VarOrImmArg &&) = default;

    VarOrImmArg(Variable &var) {
        operator=(var);
    }

    VarOrImmArg(uint32_t imm) {
        operator=(imm);
    }

    VarOrImmArg &operator=(const VarOrImmArg &) = default;
    VarOrImmArg &operator=(VarOrImmArg &&) = default;

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
