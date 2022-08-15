#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

Variable &Emitter::CreateVariable(const char *name) {
    return vars.emplace_back(vars.size(), name);
}

} // namespace armajitto::ir