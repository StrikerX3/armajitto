#pragma once

#include "armajitto/guest/arm/instructions.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"
#include "defs/variable.hpp"
#include "location_ref.hpp"

#include <memory>
#include <vector>

namespace armajitto::ir {

class BasicBlock {
public:
    BasicBlock(LocationRef location)
        : m_location(location) {}

    LocationRef Location() const {
        return m_location;
    }

    arm::Condition Condition() const {
        return m_cond;
    }

    uint32_t InstructionCount() const {
        return m_instrCount;
    }

    const std::vector<std::unique_ptr<IROp>> &Ops() const {
        return m_ops;
    }

private:
    LocationRef m_location;
    arm::Condition m_cond;

    std::vector<std::unique_ptr<IROp>> m_ops;
    uint32_t m_instrCount = 0; // ARM/Thumb instructions

    friend class Emitter;
};

} // namespace armajitto::ir
