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
        // TODO: patch type?
    };

    using PrologFn = void (*)(uintptr_t blockFn);
    PrologFn prolog;
    HostCode epilog;
    HostCode exitIRQ;

    // Cached blocks by LocationRef::ToUint64()
    std::unordered_map<uint64_t, CachedBlock> blockCache;

    // Xbyak patch locations by LocationRef::ToUint64()
    std::unordered_map<uint64_t, std::vector<PatchInfo>> patches;

    // Helper function to retrieve a cached block, to be invoked by compiled code
    static HostCode GetCodeForLocation(std::unordered_map<uint64_t, CachedBlock> &blockCache, uint64_t lochash) {
        auto it = blockCache.find(lochash);
        if (it != blockCache.end()) {
            return it->second.code;
        } else {
            return CastUintPtr(nullptr);
        }
    }

    // Retrieves the cached block for the specified location, or nullptr if no block was compiled there.
    HostCode GetCodeForLocation(LocationRef loc) {
        return GetCodeForLocation(blockCache, loc.ToUint64());
    }

    void Clear() {
        blockCache.clear();
        patches.clear();
        prolog = nullptr;
        epilog = HostCode(nullptr);
        exitIRQ = HostCode(nullptr);
    }
};

} // namespace armajitto::x86_64
