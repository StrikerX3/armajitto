#pragma once

#include "armajitto/core/options.hpp"

#include "basic_block.hpp"

#include <memory_resource>

namespace armajitto::ir {

class Optimizer {
public:
    Optimizer(Options::Optimizer &options, std::pmr::memory_resource &pmrBuffer)
        : m_options(options)
        , m_pmrBuffer(pmrBuffer) {}

    bool Optimize(BasicBlock &block);

private:
    Options::Optimizer &m_options;

    std::pmr::memory_resource &m_pmrBuffer;
};

} // namespace armajitto::ir
