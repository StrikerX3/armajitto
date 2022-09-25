#pragma once

#include "core/location_ref.hpp"
#include "host/host_code.hpp"
#include "util/pointer_cast.hpp"
#include "util/two_level_array.hpp"

#include <cstdint>
#include <map> // TODO: I'll probably regret this...

namespace armajitto::x86_64 {

struct CompiledCode {
    using PrologFn = int64_t (*)(HostCode blockFn, uint64_t cycles);
    PrologFn prolog;
    HostCode epilog;
    HostCode irqEntry;

    bool enableBlockLinking;

    // -------------------------------------------------------------------------
    // Cached blocks

    struct CachedBlock {
        HostCode code = nullptr;
        uint32_t guestCodeSize = 0;
    };

    // Cached blocks by LocationRef::ToUint64()
    util::TwoLevelArray<uint64_t, HostCode, 38> blockCache;
    std::map<uint64_t, CachedBlock> cachedBlocks; // for faster lookups when invalidating code

    // -------------------------------------------------------------------------
    // Direct link patches

    struct PatchInfo {
        uint64_t cachedBlockKey;
        const uint8_t *codePos;
        const uint8_t *codeEnd;
        // TODO: patch type?
    };

    // Xbyak patch locations by LocationRef::ToUint64()
    std::multimap<uint64_t, PatchInfo> pendingPatches;
    std::multimap<uint64_t, PatchInfo> appliedPatches;

    // -------------------------------------------------------------------------
    // Memory generation tracker
    // Used to invalidate modified blocks

    static constexpr uint32_t kPageShift = 10;
    static constexpr uint32_t kPageCount = 1u << (32u - kPageShift);
    alignas(16) std::array<uint32_t, kPageCount> memPageGenerations;

    CompiledCode() {
        memPageGenerations.fill(0);
    }

    // Retrieves the cached host code pointer for the specified location, or nullptr if no block was compiled there.
    HostCode GetCodeForLocation(LocationRef loc) {
        auto *ptr = blockCache.Get(loc.ToUint64());
        if (ptr != nullptr) {
            return *ptr;
        } else {
            return nullptr;
        }
    }

    void Clear() {
        for (auto &block : cachedBlocks) {
            const LocationRef loc{block.first};
            const uint32_t start = loc.BaseAddress();
            const uint32_t end = start + block.second.guestCodeSize - 1;
            const uint32_t startPage = start >> kPageShift;
            const uint32_t endPage = end >> kPageShift;
            for (uint32_t page = startPage; page <= endPage; page++) {
                memPageGenerations[page] = 0;
            }
        }
        cachedBlocks.clear();
        blockCache.Clear();
        pendingPatches.clear();
        appliedPatches.clear();
        prolog = nullptr;
        epilog = nullptr;
        irqEntry = nullptr;
    }
};

} // namespace armajitto::x86_64
