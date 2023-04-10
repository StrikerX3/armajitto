#pragma once

#include <cstdint>

namespace armajitto {

// Recompiler parameters
struct Options {
    // Options for the translation stage
    struct Translator {
        // Specifies the maximum number of instructions to translate into a basic block.
        uint32_t maxBlockSize = 32;

        enum class CycleCountingMethod {
            // Each instruction takes a fixed amount of cycles to execute.
            InstructionFixed,

            // Compute S/N/I cycles, assuming all memory accesses take a constant number of cycles.
            SubinstructionFixed,

            // TODO: implement this
            // Compute S/N/I cycles using a memory access timing table.
            // SubinstructionTimingTable,
        };

        // Specifies how the translator counts cycles.
        CycleCountingMethod cycleCountingMethod = CycleCountingMethod::InstructionFixed;

        // Number of cycles per instruction.
        // Used when method == CycleCountingMethod::InstructionFixed.
        uint64_t cyclesPerInstruction = 2;

        // Number of cycles per memory access.
        // Used when method == CycleCountingMethod::SubinstructionFixed.
        uint64_t cyclesPerMemoryAccess = 1;
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

            bool varLifetimeOptimization = true;

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

                varLifetimeOptimization = enabled;
            }
        } passes;

        // Maximum number of optimization iterations to perform
        uint8_t maxIterations = 20;
    } optimizer;

    // Options for the host compiler stage
    struct Compiler {
        static constexpr size_t kDefaultBufferCodeSize = static_cast<size_t>(1) * 1024 * 1024;
        static constexpr size_t kDefaultMaxBufferCodeSize = static_cast<size_t>(1024) * 1024 * 1024;

        // Initial size of the code buffer
        size_t initialCodeBufferSize = kDefaultBufferCodeSize;

        // Maximum size of the code buffer
        size_t maximumCodeBufferSize = kDefaultMaxBufferCodeSize;

        // Enables block linking, which can significantly speed up execution
        // This option only takes effect on construction or after invoking Host::Clear()
        bool enableBlockLinking = true;
    } compiler;
};

} // namespace armajitto
