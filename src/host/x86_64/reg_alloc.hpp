#pragma once

#include "abi.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "armajitto/ir/ir_ops.hpp"
#include "var_lifetime.hpp"

#include <xbyak/xbyak.h>

#include <unordered_map>

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

private:
    Xbyak::CodeGenerator &m_code;
    VarLifetimeTracker m_varLifetimes;

    static constexpr std::array<Xbyak::Reg32, 10> kFreeRegs = {/*ecx,*/ edi, esi,  r8d,  r9d,  r10d,
                                                               r11d,         r12d, r13d, r14d, r15d};
    // FIXME: this is a HACK to get things going
    size_t m_next = 0;
    std::unordered_map<size_t, Xbyak::Reg32> m_allocatedRegs;

    void Release(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::x86_64
