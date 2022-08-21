#pragma once

#include "armajitto/guest/arm/gpr.hpp"
#include "armajitto/guest/arm/mode.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace armajitto::ir {

struct GPRArg {
    arm::GPR gpr;

    GPRArg(arm::GPR gpr)
        : gpr(gpr)
        , mode(arm::Mode::User) {}

    GPRArg(arm::GPR gpr, arm::Mode mode)
        : gpr(gpr)
        , mode(ResolveMode(gpr, mode)) {}

    arm::Mode Mode() const {
        return mode;
    }

    std::string ToString() const {
        return arm::ToString(gpr) + "_" + arm::ToString(mode);
    }

private:
    arm::Mode mode;

    static constexpr auto modeMap = [] {
        std::array<std::array<arm::Mode, 32>, 16> modeMap{};
        for (auto &row : modeMap) {
            row.fill(arm::Mode::User);
        }
        for (size_t reg = 8; reg <= 12; reg++) {
            auto &row = modeMap[reg];
            row[static_cast<size_t>(arm::Mode::FIQ)] = arm::Mode::FIQ;
        }
        for (size_t reg = 13; reg <= 14; reg++) {
            auto &row = modeMap[reg];
            row[static_cast<size_t>(arm::Mode::FIQ)] = arm::Mode::FIQ;
            row[static_cast<size_t>(arm::Mode::IRQ)] = arm::Mode::IRQ;
            row[static_cast<size_t>(arm::Mode::Supervisor)] = arm::Mode::Supervisor;
            row[static_cast<size_t>(arm::Mode::Abort)] = arm::Mode::Abort;
            row[static_cast<size_t>(arm::Mode::Undefined)] = arm::Mode::Undefined;
        }

        return modeMap;
    }();

    static constexpr arm::Mode ResolveMode(arm::GPR gpr, arm::Mode mode) {
        auto gprIndex = static_cast<size_t>(gpr);
        auto modeIndex = static_cast<size_t>(mode);
        return modeMap[gprIndex][modeIndex];
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

    bool operator==(const VariableArg &) const = default;

    bool operator==(const Variable &var) const {
        return this->var == var;
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

    bool operator==(const ImmediateArg &) const = default;

    bool operator==(uint32_t value) const {
        return this->value == value;
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

    bool operator==(const VarOrImmArg &) const = default;

    bool operator==(const VariableArg &var) const {
        return !immediate && this->var == var;
    }

    bool operator==(const ImmediateArg &imm) const {
        return immediate && this->imm == imm;
    }

    bool operator==(const Variable &var) const {
        return !immediate && this->var == var;
    }

    bool operator==(uint32_t imm) const {
        return immediate && this->imm == imm;
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
