#include "reg_alloc.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

namespace armajitto::x86_64 {

// TODO: include ECX
// NOTE: R10 is used for the cycle counter
inline constexpr auto kAvailableRegs = {/*ecx,*/ edx,   esi,  edi,  r8d,  r9d,
                                        /*r10d,*/ r11d, r12d, r13d, r14d, r15d};

inline uint32_t SpillSlotOffset(size_t spillSlot) {
    return spillSlot * sizeof(uint32_t);
}

RegisterAllocator::RegisterAllocator(Xbyak::CodeGenerator &code, std::pmr::memory_resource &alloc)
    : m_codegen(code)
    , m_varLifetimes(alloc)
    , m_varAllocStates(&alloc) {}

void RegisterAllocator::Analyze(const ir::BasicBlock &block) {
    m_freeRegs.Clear();
    for (auto reg : kAvailableRegs) {
        m_freeRegs.Push(reg);
    }
    m_freeSpillSlots.Clear();
    for (uint32_t i = 0; i < abi::kMaxSpilledRegs; i++) {
        m_freeSpillSlots.Push(i);
    }

    m_varAllocStates.resize(block.VariableCount());
    m_varLifetimes.Analyze(block);
    m_regToVar.fill({});
    m_lruRegs.fill({});
    m_mostRecentReg = nullptr;
    m_leastRecentReg = nullptr;
}

void RegisterAllocator::SetInstruction(const ir::IROp *op) {
    m_currOp = op;
}

Xbyak::Reg32 RegisterAllocator::Get(ir::Variable var) {
    if (!var.IsPresent()) {
        throw std::runtime_error("Attempted to allocate a register to an absent variable");
    }

    const auto varIndex = var.Index();
    auto &entry = m_varAllocStates[varIndex];
    if (entry.allocated) {
        // Variable is allocated
        if (entry.spillSlot != ~0) {
            // Variable was spilled; bring it back to a register
            entry.reg = AllocateRegister();
            m_codegen.mov(entry.reg, dword[abi::kVarSpillBaseReg + SpillSlotOffset(entry.spillSlot)]);
            m_freeSpillSlots.Push(entry.spillSlot);
            entry.spillSlot = ~0;
        }
    } else {
        // Variable is not allocated; allocate now
        entry.reg = AllocateRegister();
        entry.allocated = true;
        entry.spillSlot = ~0;
    }

    m_regToVar[entry.reg.getIdx()] = var;
    m_regsInUse.set(entry.reg.getIdx());
    UpdateLRUQueue(entry.reg.getIdx());
    return entry.reg;
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    auto reg = AllocateRegister();
    m_tempRegs.Push(reg);
    m_regsInUse.set(reg.getIdx());
    UpdateLRUQueue(reg.getIdx());
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
    if (!m_tempRegs.Erase(tmpReg)) {
        return false;
    }

    // Turn temporary register into "permanent" by assigning it to the variable
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
        m_regsInUse.set(reg.getIdx(), false);
        RemoveFromLRUQueue(reg.getIdx());
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
        throw std::runtime_error("Ran out of free registers and spill slots");
    }

    // Ensure we have a least recently used register in the queue
    assert(m_leastRecentReg != nullptr);

    // Choose a register to spill
    Xbyak::Reg reg{};
    auto *lruEntry = m_leastRecentReg;
    while (lruEntry != nullptr) {
        int idx = std::distance(&m_lruRegs[0], lruEntry);
        // Skip registers in use by the current instruction
        if (m_regsInUse.test(idx)) {
            lruEntry = lruEntry->next;
            continue;
        }

        // The register should be allocated to a variable
        assert(m_allocatedRegs.test(idx));
        assert(m_regToVar[idx].IsPresent());
        reg = Xbyak::Reg32{idx};
        break;
    }
    if (reg.isNone()) {
        throw std::runtime_error("Too many registers in use");
    }

    // Find out which variable is currently using the register
    auto varIndex = m_regToVar[reg.getIdx()].Index();

    // Spill the variable
    auto &entry = m_varAllocStates[varIndex];
    entry.spillSlot = m_freeSpillSlots.Pop();
    m_codegen.mov(dword[abi::kVarSpillBaseReg + SpillSlotOffset(entry.spillSlot)], entry.reg);
    return reg.cvt32();
}

void RegisterAllocator::UpdateLRUQueue(int regIdx) {
    auto *lruEntry = &m_lruRegs[regIdx];
    if (lruEntry == m_mostRecentReg) {
        return;
    }

    // Remove entry from its current position
    if (lruEntry->prev != nullptr) {
        lruEntry->prev->next = lruEntry->next;
    }
    if (lruEntry->next != nullptr) {
        lruEntry->next->prev = lruEntry->prev;
    }
    if (lruEntry != m_mostRecentReg) {
        lruEntry->prev = m_mostRecentReg;
    }

    // Update least recently used register
    if (m_leastRecentReg == nullptr) {
        m_leastRecentReg = lruEntry;
    } else if (m_leastRecentReg == lruEntry) {
        m_leastRecentReg = m_leastRecentReg->next;
    }
    lruEntry->next = nullptr;

    // Update most recently used register
    if (m_mostRecentReg != nullptr && m_mostRecentReg != lruEntry) {
        m_mostRecentReg->next = lruEntry;
    }
    m_mostRecentReg = lruEntry;
}

void RegisterAllocator::RemoveFromLRUQueue(int regIdx) {
    auto *lruEntry = &m_lruRegs[regIdx];

    if (m_leastRecentReg == lruEntry) {
        m_leastRecentReg = lruEntry->next;
    }
    if (m_mostRecentReg == lruEntry) {
        m_mostRecentReg = lruEntry->prev;
    }
    if (lruEntry->next != nullptr) {
        lruEntry->next->prev = lruEntry->prev;
    }
    if (lruEntry->prev != nullptr) {
        lruEntry->prev->next = lruEntry->next;
    }
    lruEntry->next = nullptr;
    lruEntry->prev = nullptr;
}

void RegisterAllocator::Release(ir::Variable var, const ir::IROp *op) {
    // Deallocate if allocated
    const auto varIndex = var.Index();
    auto &entry = m_varAllocStates[varIndex];
    if (entry.allocated) {
        // Deallocate register
        if (m_varLifetimes.IsEndOfLife(var, op)) {
            entry.allocated = false;
            if (entry.spillSlot == ~0) {
                m_freeRegs.Push(entry.reg);
                m_allocatedRegs.set(entry.reg.getIdx(), false);
                m_regToVar[entry.reg.getIdx()] = {};
                RemoveFromLRUQueue(entry.reg.getIdx());
            }
        }

        // Mark register as not in use by current op
        if (entry.spillSlot == ~0) {
            m_regsInUse.set(entry.reg.getIdx(), false);
        }
    }
}

} // namespace armajitto::x86_64
