#include "armajitto/core/ir/ops/ir_reg_ops.hpp"

namespace armajitto::ir {

std::string IRLoadGPROp::ToString() const {
    // TODO: implement
    return "ld.gpr";
}

std::string IRStoreGPROp::ToString() const {
    // TODO: implement
    return "st.gpr";
}

std::string IRLoadPSROp::ToString() const {
    // TODO: implement
    return "ld.psr";
}

std::string IRStorePSROp::ToString() const {
    // TODO: implement
    return "st.psr";
}

} // namespace armajitto::ir
