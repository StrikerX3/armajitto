#pragma once

#include "abi.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "armajitto/ir/ir_ops.hpp"
#include "armajitto/ir/var_lifetime.hpp"
#include "armajitto/util/unsafe_circular_buffer.hpp"

#include <xbyak/xbyak.h>

#include <bitset>
#include <memory_resource>
#include <optional>
#include <vector>

namespace armajitto::x86_64 {

class RegisterAllocator {
public:
    RegisterAllocator(Xbyak::CodeGenerator &code, std::pmr::memory_resource &alloc);

    // Analyzes the given basic block, building the variable lifetime table.
    void Analyze(const ir::BasicBlock &block);

    // Sets the current instruction being compiled.
    void SetInstruction(const ir::IROp *op);

    // Retrieves the register allocated to the specified variable, or allocates one if the variable was never assigned
    // to a register.
    // If the variable is absent, throws an exception.
    // May spill over the value of a variable that is no longer in use.
    Xbyak::Reg32 Get(ir::Variable var);

    // Retrieves a temporary register without assigning it to any variable.
    Xbyak::Reg32 GetTemporary();

    // Attempts to reassign the source variable's register or spill slot to the destination variable.
    // This is only possible if the source variable is at the end of its lifetime and the destination variable is yet to
    // be assigned a register.
    // This method must be invoked after Get(src) and before Get(dst)
    void Reuse(ir::Variable dst, ir::Variable src);

    // Attempts to reassign the source variable to the destination variable and returns either the reassigned register
    // or a newly assigned register.
    // Shorthand for Reuse(dst, src) followed by Get(dst).
    Xbyak::Reg32 ReuseAndGet(ir::Variable dst, ir::Variable src) {
        Reuse(dst, src);
        return Get(dst);
    }

    // Assigns the specified temporary register to the variable.
    // Returns true if successful.
    bool AssignTemporary(ir::Variable var, Xbyak::Reg32 tmpReg);

    // Retrieves the RCX register, spilling out any associated variables if necessary.
    Xbyak::Reg64 GetRCX();

    // Releases the variables whose lifetimes expired at the specified IR instruction.
    void ReleaseVars();

    // Releases all temporarily allocated registers.
    void ReleaseTemporaries();

    // Determines if the specified register is allocated.
    bool IsRegisterAllocated(Xbyak::Reg reg) const;

private:
    Xbyak::CodeGenerator &m_codegen;
    ir::VarLifetimeTracker m_varLifetimes;

    const ir::IROp *m_currOp = nullptr;

    // -------------------------------------------------------------------------
    // Register allocation

    util::CircularBuffer<Xbyak::Reg32, 16> m_freeRegs;
    util::CircularBuffer<Xbyak::Reg32, 16> m_tempRegs;
    util::CircularBuffer<uint32_t, abi::kMaxSpilledRegs + 1> m_freeSpillSlots;
    std::array<ir::Variable, 16> m_regToVar;

    std::bitset<16> m_allocatedRegs;
    std::bitset<16> m_regsInUse;

    struct LRUEntry {
        LRUEntry *prev = nullptr;
        LRUEntry *next = nullptr;
    };

    LRUEntry *m_mostRecentReg = nullptr;
    LRUEntry *m_leastRecentReg = nullptr;

    alignas(16 * 16) std::array<LRUEntry, 16> m_lruRegs;

    Xbyak::Reg32 AllocateRegister();

    void UpdateLRUQueue(int regIdx);
    void RemoveFromLRUQueue(int regIdx);

    // -------------------------------------------------------------------------
    // Variable allocation states

    struct VarAllocState {
        bool allocated = false;
        Xbyak::Reg32 reg;

        size_t spillSlot = ~0; // ~0 means "not spilled"
    };
    std::pmr::vector<VarAllocState> m_varAllocStates;

    void Release(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::x86_64
