#include "armajitto/guest/arm/coprocessors/cp15/cp15_tcm.hpp"

#include "armajitto/util/bit_ops.hpp"

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
    if (memMap != nullptr) {
        memMap->codeRead.Unmap(kITCMLayer, 0, itcmReadSize);
        memMap->dataRead.Unmap(kITCMLayer, 0, itcmReadSize);
        memMap->dataWrite.Unmap(kITCMLayer, 0, itcmWriteSize);
    }

    if (enable) {
        itcmWriteSize = 0x200 << ((itcmParams >> 1) & 0x1F);
        itcmReadSize = load ? 0 : itcmWriteSize;

        if (memMap != nullptr) {
            memMap->codeRead.Map(kITCMLayer, 0, itcmReadSize, itcm, itcmSize);
            memMap->dataRead.Map(kITCMLayer, 0, itcmReadSize, itcm, itcmSize);
            memMap->dataWrite.Map(kITCMLayer, 0, itcmWriteSize, itcm, itcmSize);
        }
    } else {
        itcmWriteSize = itcmReadSize = 0;
    }
}

void TCM::SetupDTCM(bool enable, bool load) {
    if (memMap != nullptr) {
        memMap->dataRead.Unmap(kDTCMLayer, 0, dtcmReadSize);
        memMap->dataWrite.Unmap(kDTCMLayer, 0, dtcmWriteSize);
    }

    if (enable) {
        dtcmBase = dtcmParams & 0xFFFFF000;
        dtcmWriteSize = 0x200 << ((dtcmParams >> 1) & 0x1F);
        dtcmReadSize = load ? 0 : dtcmWriteSize;

        if (memMap != nullptr) {
            memMap->dataRead.Map(kDTCMLayer, dtcmBase, dtcmReadSize, dtcm, dtcmSize);
            memMap->dataWrite.Map(kDTCMLayer, dtcmBase, dtcmWriteSize, dtcm, dtcmSize);
        }
    } else {
        dtcmBase = 0xFFFFFFFF;
        dtcmWriteSize = dtcmReadSize = 0;
    }
}

} // namespace armajitto::arm::cp15
