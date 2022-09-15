#pragma once

#include "system.hpp"

#include <memory>

class Interpreter {
public:
    virtual ~Interpreter() = default;

    virtual void Reset() = 0;

    virtual void JumpTo(uint32_t address, bool thumb) = 0;
    virtual void Run(uint64_t numCycles) = 0;

    virtual bool &IRQLine() = 0;

    virtual uint32_t &GPR(armajitto::arm::GPR gpr) = 0;
    virtual uint32_t &GPR(armajitto::arm::GPR gpr, armajitto::arm::Mode mode) = 0;

    virtual uint32_t GetCPSR() const = 0;
    virtual uint32_t GetSPSR() const = 0;
    virtual void SetCPSR(uint32_t value) = 0;
    virtual void SetSPSR(armajitto::arm::Mode mode, uint32_t value) = 0;
};

std::unique_ptr<Interpreter> MakeARM946ESInterpreter(FuzzerSystem &sys);
