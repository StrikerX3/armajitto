#include "reg_alloc.hpp"

namespace armajitto::x86_64 {

std::optional<Xbyak::Reg32> RegisterAllocator::Get(ir::Variable var) {
    if (!var.IsPresent()) {
        return std::nullopt;
    }

    auto it = m_allocatedRegs.find(var.Index());
    if (it != m_allocatedRegs.end()) {
        return it->second;
    }

    // TODO: implement properly -- should check for free registers, spill vars, etc.
    auto reg = abi::kNonvolatileRegs[m_next++];
    if (m_next == abi::kNonvolatileRegs.size()) {
        m_next = 0;
    }
    m_allocatedRegs.insert({var.Index(), reg.cvt32()});
    return reg.cvt32();
}

Xbyak::Reg32 RegisterAllocator::GetTemporary() {
    // TODO: implement
    return r9d;
}

Xbyak::Reg32 RegisterAllocator::Reuse(ir::Variable dst, ir::Variable src) {
    // TODO: implement
    return r8d;
}

void RegisterAllocator::Release(ir::Variable var) {
    // TODO: implement
}

} // namespace armajitto::x86_64
