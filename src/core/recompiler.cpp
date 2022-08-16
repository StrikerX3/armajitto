#include "armajitto/core/recompiler.hpp"

namespace armajitto {

uint64_t Recompiler::Run(uint64_t minCycles) {
    uint64_t cyclesExecuted = 0;
    while (cyclesExecuted < minCycles) {
        // TODO: do the JIT magic here
        // - check for cached block; if not found, translate/optimize/compile block
        // - execute cached block

        ++cyclesExecuted;
    }
    return cyclesExecuted;
}

} // namespace armajitto
