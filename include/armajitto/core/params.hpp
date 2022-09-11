#pragma once

#include <cstdint>

namespace armajitto {

// Parameters for the translation stage of the recompiler.
struct TranslatorParameters {
    // Specifies the maximum number of instructions to translate into a basic block.
    uint32_t maxBlockSize = 32;
};

// Parameters for the optimization stage of the recompiler.
struct OptimizerParameters {
    // Specifies which optimization passes to perform.
    struct Passes {
        bool constantPropagation = true;

        bool deadRegisterStoreElimination = true;
        bool deadGPRStoreElimination = true;
        bool deadHostFlagStoreElimination = true;
        bool deadFlagValueStoreElimination = true;
        bool deadVariableStoreElimination = true;

        bool bitwiseOpsCoalescence = true;
        bool arithmeticOpsCoalescence = true;
        bool hostFlagsOpsCoalescence = true;
    } passes;

    // Maximum number of optimization iterations to perform
    uint8_t maxIterations = 20;
};

} // namespace armajitto
