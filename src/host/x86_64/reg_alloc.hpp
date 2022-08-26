#pragma once

#include "abi.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "armajitto/ir/ir_ops.hpp"
#include "var_lifetime.hpp"

#include <xbyak/xbyak.h>

#include <deque>
#include <optional>
#include <vector>

namespace armajitto::x86_64 {

class RegisterAllocator {
public:
    RegisterAllocator(Xbyak::CodeGenerator &code);

    void Analyze(const ir::BasicBlock &block);

    // Retrieves the register allocated to the specified variable, or allocates one if the variable was never assigned
    // to a register.
    // If the variable is absent, throws an exception.
    // May spill over the value of a variable that is no longer in use.
    Xbyak::Reg32 Get(ir::Variable var);

    // Retrieves a temporary register without assigning it to any variable.
    Xbyak::Reg32 GetTemporary();

    // Retrieves the RCX register, spilling out any associated variables if necessary.
    Xbyak::Reg64 GetRCX();

    // Releases the variables whose lifetimes expired at the specified IR instruction.
    void ReleaseVars(const ir::IROp *op);

    // Releases all temporarily allocated registers.
    void ReleaseTemporaries();

private:
    Xbyak::CodeGenerator &m_code;
    VarLifetimeTracker m_varLifetimes;

    // -------------------------------------------------------------------------
    // Free registers

    std::deque<Xbyak::Reg32> m_freeRegs;
    std::deque<Xbyak::Reg32> m_tempRegs;

    Xbyak::Reg32 AllocateRegister();

    // -------------------------------------------------------------------------
    // Variable allocation states

    struct VarAllocState {
        bool allocated = false;
        Xbyak::Reg32 reg;

        size_t spillSlot = ~0; // ~0 means "not spilled"
    };
    std::vector<VarAllocState> m_varAllocStates;

    void Release(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::x86_64
