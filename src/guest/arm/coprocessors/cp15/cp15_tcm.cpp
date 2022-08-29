#include "armajitto/guest/arm/coprocessors/cp15/cp15_tcm.hpp"

#include "armajitto/util/bit_ops.hpp"

namespace armajitto::arm::cp15 {

void TCM::Reset() {
    std::fill(itcm.begin(), itcm.end(), 0);
    std::fill(dtcm.begin(), dtcm.end(), 0);

    itcmWriteSize = itcmReadSize = 0;
    itcmParams = 0;

    dtcmBase = 0xFFFFFFFF;
    dtcmWriteSize = dtcmReadSize = 0;
    dtcmParams = 0;
}

void TCM::Configure(const Configuration &config) {
    auto calcSize = [](uint32_t size, std::vector<uint8_t> &tcm) -> std::pair<bool, tcm::Size> {
        if (size == 0) {
            tcm.clear();
            return {true, tcm::Size::_0KB};
        } else {
            const uint32_t roundedSize = std::clamp(bit::bitceil(size), 4096u, 1048576u);
            tcm.resize(roundedSize);
            return {false, static_cast<tcm::Size>(roundedSize)};
        }
    };

    auto [itcmAbsent, itcmSize] = calcSize(config.itcmSize, itcm);
    params.itcmAbsent = itcmAbsent;
    params.itcmSize = itcmSize;
    itcm.shrink_to_fit();

    auto [dtcmAbsent, dtcmSize] = calcSize(config.dtcmSize, dtcm);
    params.dtcmAbsent = dtcmAbsent;
    params.dtcmSize = dtcmSize;
    dtcm.shrink_to_fit();

    Reset();
}

void TCM::Disable() {
    itcm.clear();
    itcm.shrink_to_fit();

    dtcm.clear();
    dtcm.shrink_to_fit();
}

void TCM::SetupITCM(bool enable, bool load) {
    if (enable) {
        itcmWriteSize = 0x200 << ((itcmParams >> 1) & 0x1F);
        itcmReadSize = load ? 0 : itcmWriteSize;
    } else {
        itcmWriteSize = itcmReadSize = 0;
    }
}

void TCM::SetupDTCM(bool enable, bool load) {
    if (enable) {
        dtcmBase = dtcmParams & 0xFFFFF000;
        dtcmWriteSize = 0x200 << ((dtcmParams >> 1) & 0x1F);
        dtcmReadSize = load ? 0 : dtcmWriteSize;
    } else {
        dtcmBase = 0xFFFFFFFF;
        dtcmWriteSize = dtcmReadSize = 0;
    }
}

} // namespace armajitto::arm::cp15
