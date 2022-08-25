#include "reg_alloc.hpp"

namespace armajitto::x86_64 {

Xbyak::Reg32 RegisterAllocator::Get(ir::Variable var) {
    // TODO: implement
    return r8d;
}

Xbyak::Reg32 RegisterAllocator::Reuse(ir::Variable dst, ir::Variable src) {
    // TODO: implement
    return r8d;
}

void RegisterAllocator::Release(ir::Variable var) {
    // TODO: implement
}

} // namespace armajitto::x86_64
