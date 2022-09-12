#pragma once

#include "core/location_ref.hpp"

#if ARMAJITTO_USE_VTUNE
    #include <jitprofiling.h>

    #include <iomanip>
    #include <sstream>
    #include <string>
#endif

#include <cstdint>

namespace vtune {

inline char moduleName[] = "armajitto";

inline void ReportCode(uintptr_t codeStart, uintptr_t codeEnd, const char *fnName) {
#if ARMAJITTO_USE_VTUNE
    if (iJIT_IsProfilingActive() != iJIT_SAMPLING_ON) {
        return;
    }

    auto methodName = std::string("armajitto::jit::") + fnName;

    iJIT_Method_Load_V2 method = {0};
    method.method_id = iJIT_GetNewMethodID();
    method.method_name = const_cast<char *>(methodName.c_str());
    method.method_load_address = reinterpret_cast<void *>(codeStart);
    method.method_size = static_cast<unsigned int>(codeEnd - codeStart);
    method.module_name = moduleName;
    iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, static_cast<void *>(&method));
#endif
}

inline void ReportBasicBlock(uintptr_t codeStart, uintptr_t codeEnd, armajitto::LocationRef loc) {
#if ARMAJITTO_USE_VTUNE
    if (iJIT_IsProfilingActive() != iJIT_SAMPLING_ON) {
        return;
    }

    std::ostringstream osFnName{};
    osFnName << "block_" << std::setfill('0') << std::setw(8) << std::right << std::hex << std::uppercase << loc.PC();

    using armajitto::arm::Mode;
    switch (loc.Mode()) {
    case Mode::User: osFnName << "_USR_"; break;
    case Mode::FIQ: osFnName << "_FIQ_"; break;
    case Mode::IRQ: osFnName << "_IRQ_"; break;
    case Mode::Supervisor: osFnName << "_SVC_"; break;
    case Mode::Abort: osFnName << "_ABT_"; break;
    case Mode::Undefined: osFnName << "_UND_"; break;
    case Mode::System: osFnName << "_SYS_"; break;
    default:
        osFnName << '_' << std::setfill('0') << std::setw(2) << std::right << std::hex << std::uppercase
                 << static_cast<uint32_t>(loc.Mode()) << '_';
        break;
    }

    if (loc.IsThumbMode()) {
        osFnName << "Thumb";
    } else {
        osFnName << "ARM";
    }

    std::string fnName = osFnName.str();
    ReportCode(codeStart, codeEnd, fnName.c_str());
#endif
}

} // namespace vtune
