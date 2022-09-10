#pragma once

namespace armajitto::arm {

enum class Exception {
    Reset,
    UndefinedInstruction,
    SoftwareInterrupt, // SWI
    PrefetchAbort,
    DataAbort,
    AddressExceeds26bit,
    NormalInterrupt, // IRQ
    FastInterrupt,   // FIQ
};

} // namespace armajitto::arm
