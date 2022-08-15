#include "armajitto/core/ir/ops/ir_mem_ops.hpp"

namespace armajitto::ir {

std::string IRMemReadOp::ToString() const {
    // TODO: implement
    return "ld.mem";
}

std::string IRMemWriteOp::ToString() const {
    // TODO: implement
    return "st.mem";
}

} // namespace armajitto::ir
