#include "reg_alloc.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::x86_64 {

RegisterAllocator::RegisterAllocator(Xbyak::CodeGenerator &code, std::pmr::memory_resource &alloc)
    : m_code(code)
    , m_varLifetimes(alloc)
    , m_varAllocStates(&alloc) {

    // TODO: include ECX
    for (auto reg : {/*ecx,*/ edi, esi, r8d, r9d, r10d, r11d, r12d, r13d, r14d, r15d}) {
        m_freeRegs.Push(reg);
    }
    for (uint32_t i = 0; i < abi::kMaxSpilledRegs; i++) {
        m_freeSpillSlots.Push(i);
    }
}

void RegisterAllocator::Analyze(const ir::BasicBlock &block) {
    m_varAllocStates.resize(block.VariableCount());
    m_varLifetimes.Analyze(block);
}

void RegisterAllocator::SetInstruction(const ir::IROp *op) {
    m_currOp = op;
}

Xbyak::Reg32 RegisterAllocator::Get(ir::Variable var) {
    if (!var.IsPresent()) {
        throw std::runtime_error("attempted to allocate a register to an absent variable");
    }

    const auto varIndex = var.Index();
    auto &entry = m_varAllocStates[varIndex];
    if (entry.allocated) {
        // Variable is allocated
        if (entry.spillSlot != ~0) {
            // Variable was spilled; bring it back to a register
            entry.reg = AllocateRegister();
            m_code.mov(entry.reg, dword[rsp - entry.spillSlot * sizeof(uint32_t)]);
            entry.spillSlot = ~0;
        }
    } else {
        // Variable is not allocated; allocate now
        entry.reg = AllocateRegister();
        entry.allocated = true;
        entry.spillSlot = ~0;
    }

    return entry.reg;
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    auto reg = AllocateRegister();
    m_tempRegs.Push(reg);
    return reg;
}

void RegisterAllocator::Reuse(ir::Variable dst, ir::Variable src) {
    // Both variables must be present
    if (!dst.IsPresent() || !src.IsPresent()) {
        return;
    }

    // Source variable is still used elsewhere
    if (!m_varLifetimes.IsEndOfLife(src, m_currOp)) {
        return;
    }

    const auto srcIndex = src.Index();
    const auto dstIndex = dst.Index();
    auto &srcEntry = m_varAllocStates[srcIndex];
    auto &dstEntry = m_varAllocStates[dstIndex];

    // src must be allocated and dst must be deallocated for the copy to happen
    if (!srcEntry.allocated || dstEntry.allocated) {
        return;
    }

    // Copy allocation and mark src as deallocated
    dstEntry = srcEntry;
    srcEntry.allocated = false;
}

bool RegisterAllocator::AssignTemporary(ir::Variable var, Xbyak::Reg32 tmpReg) {
    // Do nothing if there is no variable
    if (!var.IsPresent()) {
        return false;
    }

    // Can't assign to a variable if already was assigned a register
    const auto varIndex = var.Index();
    auto &entry = m_varAllocStates[varIndex];
    if (entry.allocated) {
        return false;
    }

    // Check if the register was temporarily allocated
    auto it = m_tempRegs.Find(tmpReg);
    if (it == m_tempRegs.Capacity()) {
        return false;
    }

    // Turn temporary register into "permanent" by assigning it to the variable
    m_tempRegs.Erase(it);
    entry.allocated = true;
    entry.reg = tmpReg;
    return true;
}

Xbyak::Reg64 RegisterAllocator::GetRCX() {
    // TODO: implement
    return rcx;
}

void RegisterAllocator::ReleaseVars() {
    ir::VisitIROpVars(m_currOp, [this](const auto *op, ir::Variable var, bool) -> void { Release(var, op); });
}

void RegisterAllocator::ReleaseTemporaries() {
    while (!m_tempRegs.IsEmpty()) {
        auto reg = m_tempRegs.Pop();
        m_allocatedRegs.set(reg.getIdx(), false);
        m_freeRegs.Push(reg);
    }
}

bool RegisterAllocator::IsRegisterAllocated(Xbyak::Reg reg) const {
    return m_allocatedRegs.test(reg.getIdx());
}

Xbyak::Reg32 RegisterAllocator::AllocateRegister() {
    // TODO: prefer nonvolatiles if available
    if (!m_freeRegs.IsEmpty()) {
        auto reg = m_freeRegs.Pop();
        m_allocatedRegs.set(reg.getIdx());
        return reg;
    }

    // No more free registers; spill a register onto the stack
    if (m_freeSpillSlots.IsEmpty()) {
        throw std::runtime_error("No more free registers or spill slots");
    }

    // Choose a register to spill
    // TODO: use LRU queue
    // auto reg = m_usedRegs.Pop();

    // Find out which variable is currently using the register
    // TODO: reg -> variable map
    // auto varIndex = m_regToVar[reg.getIdx()];

    // Spill the variable
    // TODO: implement the above
    // auto &entry = m_varAllocStates[varIndex];
    // entry.spillSlot = m_freeSpillSlots.Pop();
    // m_code.mov(dword[rsp - entry.spillSlot * sizeof(uint32_t)], entry.reg);

    // TODO: implement spilling
    // return reg;
    throw std::runtime_error("Variable spilling is not yet implemented");
}

void RegisterAllocator::Release(ir::Variable var, const ir::IROp *op) {
    if (m_varLifetimes.IsEndOfLife(var, op)) {
        // Deallocate if allocated
        const auto varIndex = var.Index();
        auto &entry = m_varAllocStates[varIndex];
        if (entry.allocated) {
            entry.allocated = false;
            if (entry.spillSlot == ~0) {
                m_freeRegs.Push(entry.reg);
                m_allocatedRegs.set(entry.reg.getIdx(), false);
            }
        }
    }
}

} // namespace armajitto::x86_64
