#include "armajitto/core/ir/ops/ir_cop_ops.hpp"

namespace armajitto::ir {

std::string IRLoadCopRegisterOp::ToString() const {
    // TODO: implement
    return "mrc";
}

std::string IRStoreCopRegisterOp::ToString() const {
    // TODO: implement
    return "mcr";
}

} // namespace armajitto::ir
