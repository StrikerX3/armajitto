#pragma once

#include "armajitto/guest/arm/gpr.hpp"
#include "armajitto/guest/arm/mode.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <cstdint>
#include <format>
#include <string>
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

    std::string ToString() const {
        if (userMode) {
            return ::armajitto::ToString(gpr) + "_usr";
        } else {
            return ::armajitto::ToString(gpr);
        }
    }
};

struct VariableArg {
    Variable var;

    VariableArg() = default;

    VariableArg(Variable var) {
        operator=(var);
    }

    VariableArg &operator=(Variable var) {
        this->var = var;
        return *this;
    }

    std::string ToString() const {
        if (var.IsPresent()) {
            return std::format("$v{}", var.Index());
        } else {
            return std::string("$v?");
        }
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

    std::string ToString() const {
        return std::format("#0x{:x}", value);
    }
};

struct VarOrImmArg {
    bool immediate;
    VariableArg var;  // when immediate == false
    ImmediateArg imm; // when immediate == true

    VarOrImmArg() {
        operator=(0);
    }

    VarOrImmArg(Variable var) {
        operator=(var);
    }

    VarOrImmArg(uint32_t imm) {
        operator=(imm);
    }

    VarOrImmArg &operator=(Variable var) {
        immediate = false;
        this->var = var;
        return *this;
    }

    VarOrImmArg &operator=(uint32_t imm) {
        immediate = true;
        this->imm = imm;
        return *this;
    }

    std::string ToString() const {
        if (immediate) {
            return imm.ToString();
        } else {
            return var.ToString();
        }
    }
};

} // namespace armajitto::ir
