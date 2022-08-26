#include "reg_alloc.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cstdio>

namespace armajitto::x86_64 {

RegisterAllocator::RegisterAllocator(Xbyak::CodeGenerator &code)
    : m_code(code) {

    // TODO: include ECX
    for (auto reg : {/*ecx,*/ edi, esi, r8d, r9d, r10d, r11d, r12d, r13d, r14d, r15d}) {
        m_freeRegs.push_back(reg);
    }
}

void RegisterAllocator::Analyze(const ir::BasicBlock &block) {
    m_varAllocStates.resize(block.VariableCount());
    m_varLifetimes.Analyze(block);
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
        auto _varStr = var.ToString();
        printf("      assigned to %s\n", _varStr.c_str());
    }

    return entry.reg;
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    auto reg = AllocateRegister();
    printf("      temporary register\n");
    m_tempRegs.push_back(reg);
    return reg;
}

Xbyak::Reg64 RegisterAllocator::GetRCX() {
    // TODO: implement
    return rcx;
}

void RegisterAllocator::ReleaseVars(const ir::IROp *op) {
    ir::VisitIROpVars(op, [this](const auto *op, ir::Variable var, bool) -> void { Release(var, op); });
}

void RegisterAllocator::ReleaseTemporaries() {
    for (auto reg : m_tempRegs) {
        printf("    releasing temporary register %d\n", reg.getIdx());
    }
    m_freeRegs.insert(m_freeRegs.end(), m_tempRegs.begin(), m_tempRegs.end());
    m_tempRegs.clear();
    printf("    free registers list:");
    for (auto reg : m_freeRegs) {
        printf(" %d", reg.getIdx());
    }
    printf("\n");
}

Xbyak::Reg32 RegisterAllocator::AllocateRegister() {
    if (!m_freeRegs.empty()) {
        auto reg = m_freeRegs.front();
        printf("    allocating register %d\n", reg.getIdx());
        m_freeRegs.pop_front();
        return reg;
    }

    // No more free registers
    // TODO: implement spilling
    throw std::runtime_error("No more free registers; variable spilling is not yet implemented");
}

void RegisterAllocator::Release(ir::Variable var, const ir::IROp *op) {
    if (m_varLifetimes.IsEndOfLife(var, op)) {
        auto _varStr = var.ToString();
        auto _opStr = op->ToString();
        printf("    var %s expired at op %s\n", _varStr.c_str(), _opStr.c_str());

        const auto varIndex = var.Index();
        auto &entry = m_varAllocStates[varIndex];
        if (!entry.allocated) {
            // This shouldn't happen
            printf("      ...uh oh\n");
        } else {
            entry.allocated = false;
            if (entry.spillSlot == ~0) {
                m_freeRegs.push_back(entry.reg);
                printf("      returned register %d\n", entry.reg.getIdx());
            }
        }
    }
}

} // namespace armajitto::x86_64
