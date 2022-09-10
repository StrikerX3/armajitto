#include "armajitto/guest/arm/coprocessors/cp15/cp15_tcm.hpp"

#include "util/bit_ops.hpp"

#include <algorithm>

namespace armajitto::arm::cp15 {

constexpr uint8_t kITCMLayer = 2;
constexpr uint8_t kDTCMLayer = 1;

TCM::~TCM() {
    if (itcm != nullptr) {
        delete[] itcm;
    }
    if (dtcm != nullptr) {
        delete[] dtcm;
    }
}

void TCM::Reset() {
    if (itcm != nullptr) {
        std::fill_n(itcm, itcmSize, 0);
    }
    if (dtcm != nullptr) {
        std::fill_n(dtcm, dtcmSize, 0);
    }

    itcmWriteSize = itcmReadSize = 0;
    itcmParams = 0;

    dtcmBase = 0xFFFFFFFF;
    dtcmWriteSize = dtcmReadSize = 0;
    dtcmParams = 0;
}

void TCM::Configure(const Configuration &config) {
    auto calcSize = [](uint32_t size, uint8_t *&tcm, uint32_t &tcmSize) -> std::pair<bool, tcm::Size> {
        if (size == 0) {
            delete[] tcm;
            tcm = nullptr;
            tcmSize = 0;
            return {true, tcm::Size::_0KB};
        } else {
            const uint32_t roundedSize = std::clamp(bit::bitceil(size), 4096u, 1048576u);
            if (tcm != nullptr) {
                delete[] tcm;
            }
            tcm = new uint8_t[roundedSize];
            tcmSize = roundedSize;
            return {false, static_cast<tcm::Size>(roundedSize)};
        }
    };

    auto [itcmAbsent, itcmSizeParam] = calcSize(config.itcmSize, itcm, itcmSize);
    params.itcmAbsent = itcmAbsent;
    params.itcmSize = itcmSizeParam;

    auto [dtcmAbsent, dtcmSizeParam] = calcSize(config.dtcmSize, dtcm, dtcmSize);
    params.dtcmAbsent = dtcmAbsent;
    params.dtcmSize = dtcmSizeParam;

    Reset();
}

void TCM::Disable() {
    if (itcm != nullptr) {
        delete[] itcm;
    }
    itcmSize = 0;

    if (dtcm != nullptr) {
        delete[] dtcm;
    }
    dtcmSize = 0;
}

void TCM::SetupITCM(bool enable, bool load) {
    const auto prevReadSize = itcmReadSize;
    const auto prevWriteSize = itcmWriteSize;

    if (enable) {
        itcmWriteSize = 0x200 << ((itcmParams >> 1) & 0x1F);
        itcmReadSize = load ? 0 : itcmWriteSize;
    } else {
        itcmWriteSize = itcmReadSize = 0;
    }

    // Apply changes to memory map
    if (memMap != nullptr) {
        using MemArea = MemoryMap::Areas;

        if (itcmReadSize > prevReadSize) {
            // TODO: be more efficient and only map what's added, aligned to itcmSize
            memMap->Map(MemArea::AllRead, kITCMLayer, 0, itcmReadSize, itcm, itcmSize);
        } else if (itcmReadSize < prevReadSize) {
            memMap->Unmap(MemArea::AllRead, kITCMLayer, itcmReadSize, prevReadSize - itcmReadSize);
        }

        if (itcmWriteSize > prevWriteSize) {
            // TODO: be more efficient and only map what's added, aligned to itcmSize
            memMap->Map(MemArea::DataWrite, kITCMLayer, 0, itcmWriteSize, itcm, itcmSize);
        } else if (itcmWriteSize < prevWriteSize) {
            memMap->Unmap(MemArea::DataWrite, kITCMLayer, itcmWriteSize, prevWriteSize - itcmWriteSize);
        }
    }
}

void TCM::SetupDTCM(bool enable, bool load) {
    const auto prevBase = dtcmBase;
    const auto prevReadSize = dtcmReadSize;
    const auto prevWriteSize = dtcmWriteSize;

    if (enable) {
        dtcmBase = dtcmParams & 0xFFFFF000;
        dtcmWriteSize = 0x200 << ((dtcmParams >> 1) & 0x1F);
        dtcmReadSize = load ? 0 : dtcmWriteSize;
    } else {
        dtcmBase = 0xFFFFFFFF;
        dtcmWriteSize = dtcmReadSize = 0;
    }

    // Apply changes to the memory map
    if (memMap != nullptr) {
        using MemArea = MemoryMap::Areas;

        if (prevBase != dtcmBase) {
            memMap->Unmap(MemArea::DataRead, kDTCMLayer, prevBase, prevReadSize);
            memMap->Map(MemArea::DataRead, kDTCMLayer, dtcmBase, dtcmReadSize, dtcm, dtcmSize);

            memMap->Unmap(MemArea::DataWrite, kDTCMLayer, prevBase, prevWriteSize);
            memMap->Map(MemArea::DataWrite, kDTCMLayer, dtcmBase, dtcmWriteSize, dtcm, dtcmSize);
        } else {
            if (dtcmReadSize > prevReadSize) {
                // TODO: be more efficient and only map what's added, aligned to dtcmSize
                memMap->Map(MemArea::DataRead, kDTCMLayer, dtcmBase, dtcmReadSize, dtcm, dtcmSize);
            } else if (dtcmReadSize < prevReadSize) {
                memMap->Unmap(MemArea::DataRead, kDTCMLayer, dtcmBase + dtcmReadSize, prevReadSize - dtcmReadSize);
            }

            if (dtcmWriteSize > prevWriteSize) {
                // TODO: be more efficient and only map what's added, aligned to dtcmSize
                memMap->Map(MemArea::DataWrite, kDTCMLayer, dtcmBase, dtcmWriteSize, dtcm, dtcmSize);
            } else if (dtcmWriteSize < prevWriteSize) {
                memMap->Unmap(MemArea::DataWrite, kDTCMLayer, dtcmBase + dtcmWriteSize, prevWriteSize - dtcmWriteSize);
            }
        }
    }
}

} // namespace armajitto::arm::cp15
