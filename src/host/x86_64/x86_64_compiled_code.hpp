#pragma once

#include "core/location_ref.hpp"
#include "host/block_cache.hpp"
#include "host/host_code.hpp"
#include "host/mem_gen_tracker.hpp"
#include "util/pointer_cast.hpp"

#include <cstdint>
#include <map> // TODO: I'll probably regret this...

namespace armajitto::x86_64 {

struct CompiledCode {
    struct PatchInfo {
        uint64_t cachedBlockKey;
        const uint8_t *codePos;
        const uint8_t *codeEnd;
        // TODO: patch type?
    };

    using PrologFn = int64_t (*)(HostCode blockFn, uint64_t cycles);
    PrologFn prolog;
    HostCode epilog;
    HostCode irqEntry;

    bool enableBlockLinking;

    // Cached blocks by LocationRef::ToUint64()
    BlockCache blockCache;

    // Xbyak patch locations by LocationRef::ToUint64()
    std::multimap<uint64_t, PatchInfo> pendingPatches;
    std::multimap<uint64_t, PatchInfo> appliedPatches;

    // Memory generation tracker; used to invalidate modified blocks
    MemoryGenerationTracker memGenTracker;
    // -------- old code start --------
    // static constexpr uint32_t kPageShift = 10;
    // static constexpr uint32_t kPageCount = 1u << (32u - kPageShift);
    // alignas(16) std::array<uint32_t, kPageCount> memPageGenerations;
    // -------- old code end --------

    // Retrieves the cached block for the specified location, or nullptr if no block was compiled there.
    HostCode GetCodeForLocation(LocationRef loc) {
        auto *entry = blockCache.Get(loc.ToUint64());
        if (entry == nullptr) {
            return nullptr;
        }
        return *entry;
    }

    void Clear() {
        blockCache.Clear();
        pendingPatches.clear();
        appliedPatches.clear();
        memGenTracker.Clear();
        // -------- old code start --------
        // memPageGenerations.fill(0);
        // -------- old code end --------
        prolog = nullptr;
        epilog = nullptr;
        irqEntry = nullptr;
    }
};

} // namespace armajitto::x86_64
