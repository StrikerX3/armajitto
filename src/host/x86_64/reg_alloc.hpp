#pragma once

#include "abi.hpp"
#include "armajitto/ir/defs/variable.hpp"

#include <optional>
#include <unordered_map>

namespace armajitto::x86_64 {

class RegisterAllocator {
public:
    // Retrieves the register allocated to the specified variable, or allocates one if the variable was never assigned
    // to a register. May spill over the value of a variable that is no longer in use.
    std::optional<Xbyak::Reg32> Get(ir::Variable var);

    // Retrieves a temporary register without assigning it to any variable.
    Xbyak::Reg32 GetTemporary();

    // Reuses the register allocated to src, assigning it to dst and releasing from src.
    Xbyak::Reg32 Reuse(ir::Variable dst, ir::Variable src);

    // Release the register assigned to var, allowing it to be used with other variables.
    void Release(ir::Variable var);

private:
    // FIXME: this is a HACK to get things going
    size_t m_next = 0;
    std::unordered_map<size_t, Xbyak::Reg32> m_allocatedRegs;
};

} // namespace armajitto::x86_64
