#pragma once

#include "armajitto/core/location_ref.hpp"
#include "armajitto/host/host_code.hpp"
#include "armajitto/util/pointer_cast.hpp"

#include <cstdint>
#include <unordered_map> // TODO: I'll probably regret this...

namespace armajitto::x86_64 {

struct CompiledCode {
    struct CachedBlock {
        HostCode code;
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

    // Cached blocks by LocationRef::ToUint64()
    std::unordered_map<uint64_t, CachedBlock> blockCache;

    // Xbyak patch locations by LocationRef::ToUint64()
    std::unordered_map<uint64_t, std::vector<PatchInfo>> pendingPatches;
    std::unordered_map<uint64_t, std::vector<PatchInfo>> appliedPatches;

    // Helper function to retrieve a cached block, to be invoked by compiled code
    static HostCode GetCodeForLocationTrampoline(std::unordered_map<uint64_t, CachedBlock> &blockCache,
                                                 uint64_t lochash) {
        auto it = blockCache.find(lochash);
        if (it != blockCache.end()) {
            return it->second.code;
        } else {
            return nullptr;
        }
    }

    // Retrieves the cached block for the specified location, or nullptr if no block was compiled there.
    HostCode GetCodeForLocation(LocationRef loc) {
        return GetCodeForLocationTrampoline(blockCache, loc.ToUint64());
    }

    void Clear() {
        blockCache.clear();
        pendingPatches.clear();
        appliedPatches.clear();
        prolog = nullptr;
        epilog = nullptr;
        irqEntry = nullptr;
    }
};

} // namespace armajitto::x86_64
