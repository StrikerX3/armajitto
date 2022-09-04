#include "reg_alloc.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::x86_64 {

// TODO: include ECX
inline constexpr auto kAvailableRegs = {/*ecx,*/ edx, esi, edi, r8d, r9d, r10d, r11d, r12d, r13d, r14d, r15d};

RegisterAllocator::RegisterAllocator(Xbyak::CodeGenerator &code, std::pmr::memory_resource &alloc)
    : m_codegen(code)
    , m_varLifetimes(alloc)
    , m_varAllocStates(&alloc) {

    for (auto reg : kAvailableRegs) {
        m_freeRegs.Push(reg);
    }
    for (uint32_t i = 0; i < abi::kMaxSpilledRegs; i++) {
        m_freeSpillSlots.Push(i);
    }
}

void RegisterAllocator::Analyze(const ir::BasicBlock &block) {
    m_varAllocStates.resize(block.VariableCount());
    m_varLifetimes.Analyze(block);
    m_regToVar.fill({});
}

void RegisterAllocator::SetInstruction(const ir::IROp *op) {
    m_currOp = op;
}

Xbyak::Reg32 RegisterAllocator::Get(ir::Variable var) {
    if (!var.IsPresent()) {
        throw std::runtime_error("Attempted to allocate a register to an absent variable");
    }

    const auto varIndex = var.Index();
    // printf("getting register for var %zu\n", varIndex);
    auto &entry = m_varAllocStates[varIndex];
    if (entry.allocated) {
        // Variable is allocated
        if (entry.spillSlot != ~0) {
            // printf("  unspilling from slot %zu\n", entry.spillSlot);
            // Variable was spilled; bring it back to a register
            entry.reg = AllocateRegister();
            m_codegen.mov(entry.reg, dword[rbp + abi::kVarSpillBaseOffset + entry.spillSlot * sizeof(uint32_t)]);
            m_freeSpillSlots.Push(entry.spillSlot);
            entry.spillSlot = ~0;
            m_regToVar[entry.reg.getIdx()] = var;
        }
    } else {
        // printf("  allocating new register\n");
        // Variable is not allocated; allocate now
        entry.reg = AllocateRegister();
        entry.allocated = true;
        entry.spillSlot = ~0;
        m_regToVar[entry.reg.getIdx()] = var;
    }

    /*if (!m_regsInUse.test(entry.reg.getIdx())) {
        printf("  register %d in use by current op\n", entry.reg.getIdx());
    }*/
    m_regsInUse.set(entry.reg.getIdx());
    return entry.reg;
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    // printf("  allocating temporary register\n");
    auto reg = AllocateRegister();
    m_tempRegs.Push(reg);
    /*if (!m_regsInUse.test(reg.getIdx())) {
        printf("  register %d in use by current op\n", reg.getIdx());
    }*/
    m_regsInUse.set(reg.getIdx());
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
    // printf("reused register %d from var %zu to var %zu\n", dstEntry.reg.getIdx(), srcIndex, dstIndex);
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
    // printf("assigned temporary register %d to var %zu\n", tmpReg.getIdx(), varIndex);
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
        /*printf("released temporary register %d\n", reg.getIdx());
        if (m_regsInUse.test(reg.getIdx())) {
            printf("  register %d no longer in use by current op\n", reg.getIdx());
        }*/
        m_regsInUse.set(reg.getIdx(), false);
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
        // printf("    allocated free register %d\n", reg.getIdx());
        return reg;
    }

    // No more free registers; spill a register onto the stack
    if (m_freeSpillSlots.IsEmpty()) {
        throw std::runtime_error("Ran out of free registers and spill slots");
    }

    // Choose a register to spill
    // TODO: use LRU queue
    Xbyak::Reg reg{};
    for (auto possibleReg : kAvailableRegs) {
        // Skip registers in use by the current instruction
        if (m_regsInUse.test(possibleReg.getIdx())) {
            continue;
        }
        // The register should be allocated to a variable
        assert(m_allocatedRegs.test(possibleReg.getIdx()));
        assert(m_regToVar[possibleReg.getIdx()].IsPresent());
        reg = possibleReg;
        break;
    }
    if (reg.isNone()) {
        throw std::runtime_error("Current op uses too many registers");
    }

    // Find out which variable is currently using the register
    auto varIndex = m_regToVar[reg.getIdx()].Index();

    // Spill the variable
    auto &entry = m_varAllocStates[varIndex];
    entry.spillSlot = m_freeSpillSlots.Pop();
    m_codegen.mov(dword[rbp + abi::kVarSpillBaseOffset + entry.spillSlot * sizeof(uint32_t)], entry.reg);
    // printf("    spilled variable %zu to slot %zu\n", varIndex, entry.spillSlot);
    // printf("    allocated register %d\n", reg.getIdx());
    return reg.cvt32();
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
                // printf("released register %d\n", entry.reg.getIdx());
            }
        }

        // Mark register as not in use by current op
        if (entry.spillSlot == ~0) {
            m_regsInUse.set(entry.reg.getIdx(), false);
            // printf("  register %d no longer in use by current op\n", entry.reg.getIdx());
        }
    }
}

} // namespace armajitto::x86_64
