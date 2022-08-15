#include "armajitto/core/recompiler.hpp"

namespace armajitto {

uint64_t Recompiler::Run(uint64_t minCycles) {
    uint64_t cyclesExecuted = 0;
    while (cyclesExecuted < minCycles) {
        // TODO: implement IR emitter as the client for the decoder
        // - block should contain an emitter

        // TODO: do the JIT magic here
        // - find cached block
        // - if found, execute
        // - otherwise, generate block

        ++cyclesExecuted;
    }
    return cyclesExecuted;
}

} // namespace armajitto
