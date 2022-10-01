#pragma once

#include "core/location_ref.hpp"
#include "host/host_code.hpp"
#include "util/pointer_cast.hpp"
#include "util/three_level_array.hpp"

#include <cstdint>
#include <map> // TODO: I'll probably regret this...

namespace armajitto::x86_64 {

struct CompiledCode {
    struct CachedBlock {
        HostCode code = nullptr;
    };

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
    util::ThreeLevelArray<uint64_t, CachedBlock, 13, 13, 12> blockCache;

    // Xbyak patch locations by LocationRef::ToUint64()
    std::multimap<uint64_t, PatchInfo> pendingPatches;
    std::multimap<uint64_t, PatchInfo> appliedPatches;

    // Memory generation tracker; used to invalidate modified blocks
    static constexpr uint32_t kPageShift = 10;
    static constexpr uint32_t kPageCount = 1u << (32u - kPageShift);
    alignas(16) std::array<uint32_t, kPageCount> memPageGenerations;

    // Retrieves the cached block for the specified location, or nullptr if no block was compiled there.
    HostCode GetCodeForLocation(LocationRef loc) {
        auto *entry = blockCache.Get(loc.ToUint64());
        if (entry == nullptr) {
            return nullptr;
        }
        return entry->code;
    }

    void Clear() {
        blockCache.Clear();
        pendingPatches.clear();
        appliedPatches.clear();
        memPageGenerations.fill(0);
        prolog = nullptr;
        epilog = nullptr;
        irqEntry = nullptr;
    }
};

} // namespace armajitto::x86_64
