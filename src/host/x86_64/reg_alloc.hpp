#pragma once

#include "abi.hpp"
#include "armajitto/ir/defs/variable.hpp"

namespace armajitto::x86_64 {

class RegisterAllocator {
public:
    // Retrieves the register allocated to the specified variable, or allocates one if the variable was never assigned
    // to a register. May spill over the value of a variable that is no longer in use.
    Xbyak::Reg32 Get(ir::Variable var);

    // Reuses the register allocated to src, assigning it to dst and releasing from src.
    Xbyak::Reg32 Reuse(ir::Variable dst, ir::Variable src);

    // Release the register assigned to var, allowing it to be used with other variables.
    void Release(ir::Variable var);

private:
};

} // namespace armajitto::x86_64
