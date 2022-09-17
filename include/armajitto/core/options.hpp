#pragma once

#include <cstdint>

namespace armajitto {

// Recompiler parameters
struct Options {
    // Options for the translation stage
    struct Translator {
        // Specifies the maximum number of instructions to translate into a basic block.
        uint32_t maxBlockSize = 32;
    } translator;

    // Options for the optimization stage
    struct Optimizer {
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

            void SetAll(bool enabled) {
                constantPropagation = enabled;

                deadRegisterStoreElimination = enabled;
                deadGPRStoreElimination = enabled;
                deadHostFlagStoreElimination = enabled;
                deadFlagValueStoreElimination = enabled;
                deadVariableStoreElimination = enabled;

                bitwiseOpsCoalescence = enabled;
                arithmeticOpsCoalescence = enabled;
                hostFlagsOpsCoalescence = enabled;
            }
        } passes;

        // Maximum number of optimization iterations to perform
        uint8_t maxIterations = 20;
    } optimizer;

    // Options for the host compiler stage
    struct Compiler {
        static constexpr size_t kDefaultBufferCodeSize = static_cast<size_t>(1) * 1024 * 1024;

        // Initial size of the code buffer
        size_t initialCodeBufferSize = kDefaultBufferCodeSize;

        // Enables block linking, which can significantly speed up execution
        // This option only takes effect on construction or after invoking Host::Clear()
        bool enableBlockLinking = true;
    } compiler;
};

} // namespace armajitto
