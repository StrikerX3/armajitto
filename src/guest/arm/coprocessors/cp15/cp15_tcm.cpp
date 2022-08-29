#include "armajitto/guest/arm/coprocessors/cp15/cp15_tcm.hpp"

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
    itcm.resize(std::min(bit::bitceil(config.itcmSize), 1u * 1024u * 1024u));
    itcm.shrink_to_fit();

    dtcm.resize(std::min(bit::bitceil(config.dtcmSize), 1u * 1024u * 1024u));
    dtcm.shrink_to_fit();

    Reset();
}

void TCM::Disable() {
    itcm.clear();
    itcm.shrink_to_fit();

    dtcm.clear();
    dtcm.shrink_to_fit();
}

} // namespace armajitto::arm::cp15
