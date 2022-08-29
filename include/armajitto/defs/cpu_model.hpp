#pragma once

namespace armajitto {

enum class CPUModel {
    ARM7TDMI, // ARMv4T + dummy CP14
    ARM946ES, // ARMv5TE + full CP15 with 32 KiB ITCM, 16 KiB DTCM and Protection Unit
};

} // namespace armajitto
