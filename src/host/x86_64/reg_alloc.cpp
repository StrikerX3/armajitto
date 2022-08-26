#include "reg_alloc.hpp"

namespace armajitto::x86_64 {

RegisterAllocator::RegisterAllocator(Xbyak::CodeGenerator &code)
    : m_code(code) {}

void RegisterAllocator::Analyze(const ir::BasicBlock &block) {
    m_varLifetimes.Analyze(block);
}

Xbyak::Reg32 RegisterAllocator::Get(ir::Variable var) {
    if (!var.IsPresent()) {
        throw std::runtime_error("attempted to allocate a register to an absent variable");
    }

    auto it = m_allocatedRegs.find(var.Index());
    if (it != m_allocatedRegs.end()) {
        return it->second;
    }

    // TODO: implement properly -- should check for free registers, spill vars, etc.
    // TODO: can use a larger set of free registers, not just nonvolatiles
    auto reg = kFreeRegs[m_next++];
    if (m_next == kFreeRegs.size()) {
        m_next = 0;
    }
    m_allocatedRegs.insert({var.Index(), reg.cvt32()});
    return reg.cvt32();
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    // TODO: implement properly
    auto reg = kFreeRegs[m_next++];
    if (m_next == kFreeRegs.size()) {
        m_next = 0;
    }
    return reg.cvt32();
}

Xbyak::Reg64 RegisterAllocator::GetRCX() {
    // TODO: implement
    return rcx;
}

Xbyak::Reg32 RegisterAllocator::Reuse(ir::Variable dst, ir::Variable src) {
    // TODO: implement
    return r8d;
}

void RegisterAllocator::Release(ir::Variable var) {
    // TODO: implement
}

} // namespace armajitto::x86_64
