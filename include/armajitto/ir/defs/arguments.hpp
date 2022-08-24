#pragma once

#include "armajitto/guest/arm/gpr.hpp"
#include "armajitto/guest/arm/mode.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <optional>
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

    size_t Index() const {
        return static_cast<size_t>(gpr) | (NormalizedIndex(mode) << 4);
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
        return var.ToString();
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

// Helper function to split a pair of VarOrImmArgs into an immediate and a variable
inline std::optional<std::pair<uint32_t, Variable>> SplitImmVarPair(const VarOrImmArg &lhs, const VarOrImmArg &rhs) {
    // Requires that the two arguments be of different types
    if (lhs.immediate == rhs.immediate) {
        return std::nullopt;
    }

    if (lhs.immediate) {
        return std::make_pair(lhs.imm.value, rhs.var.var);
    } else { // rhs.immediate
        return std::make_pair(rhs.imm.value, lhs.var.var);
    }
}

// Helper function to split a pair of VarOrImmArgs into immediate and variable argument references
inline std::optional<std::pair<ImmediateArg &, VariableArg &>> SplitImmVarArgPair(VarOrImmArg &lhs, VarOrImmArg &rhs) {
    // Requires that the two arguments be of different types
    if (lhs.immediate == rhs.immediate) {
        return std::nullopt;
    }

    if (lhs.immediate) {
        return {{lhs.imm, rhs.var}};
    } else { // rhs.immediate
        return {{rhs.imm, lhs.var}};
    }
}

} // namespace armajitto::ir
