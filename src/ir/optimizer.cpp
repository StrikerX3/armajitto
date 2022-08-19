#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"

namespace armajitto::ir {

void Optimizer::Optimize(BasicBlock &block) {
    // TODO: implement: modify block in-place

    // TODO: generic IR op arguments access/manipulation
    // - add a method to get references (pointers) to all arguments (including flags), split by reads/writes
    // - how to handle the different argument types (var/imm, var, imm)? should it look at other types too?
}

} // namespace armajitto::ir
