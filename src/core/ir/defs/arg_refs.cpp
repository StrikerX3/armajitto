#include "armajitto/core/ir/defs/arg_refs.hpp"

namespace armajitto::ir {

std::string GPRArg::ToString() {
    static constexpr const char *names[] = {"r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
                                            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
    static constexpr const char *userModeNames[] = {"r0_usr",  "r1_usr",  "r2_usr",  "r3_usr", "r4_usr",  "r5_usr",
                                                    "r6_usr",  "r7_usr",  "r8_usr",  "r9_usr", "r10_usr", "r11_usr",
                                                    "r12_usr", "r13_usr", "r14_usr", "r15_usr"};

    if (userMode) [[unlikely]] {
        return userModeNames[gpr];
    } else {
        return names[gpr];
    }
}

std::string PSRArg::ToString() {
    return spsr ? "spsr" : "cpsr";
}

std::string VariableArg::ToString() {
    return "TODO";
}

std::string ImmediateArg::ToString() {
    return std::to_string(value);
}

std::string VarOrImmArg::ToString() {
    if (immediate) {
        return std::string("imm:") + arg.imm.ToString();
    } else {
        return std::string("var:") + arg.var.ToString();
    }
}

} // namespace armajitto::ir
