#pragma once

#include "armajitto/core/params.hpp"

#include "basic_block.hpp"

#include <memory_resource>

namespace armajitto::ir {

class Optimizer {
public:
    Optimizer(std::pmr::memory_resource &pmrBuffer)
        : m_pmrBuffer(pmrBuffer) {}

    OptimizerParameters &GetParameters() {
        return m_parameters;
    }

    bool Optimize(BasicBlock &block);

private:
    OptimizerParameters m_parameters;

    std::pmr::memory_resource &m_pmrBuffer;
};

} // namespace armajitto::ir
