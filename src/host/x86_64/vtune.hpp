#pragma once

#include "core/location_ref.hpp"

#if ARMAJITTO_USE_VTUNE
    #include <jitprofiling.h>

    #include <format>
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

    std::string methodName = std::format("armajitto::jit::{}", fnName);

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

    auto modeStr = [&]() -> std::string {
        using armajitto::arm::Mode;
        switch (loc.Mode()) {
        case Mode::User: return "USR";
        case Mode::FIQ: return "FIQ";
        case Mode::IRQ: return "IRQ";
        case Mode::Supervisor: return "SVC";
        case Mode::Abort: return "ABT";
        case Mode::Undefined: return "UND";
        case Mode::System: return "SYS";
        default: return std::format("{:02X}", static_cast<uint32_t>(loc.Mode()));
        }
    }();

    auto thumbStr = loc.IsThumbMode() ? "Thumb" : "ARM";
    std::string fnName = std::format("block_{:08X}_{}_{}", loc.PC(), modeStr, thumbStr);

    ReportCode(codeStart, codeEnd, fnName.c_str());
#endif
}

} // namespace vtune
