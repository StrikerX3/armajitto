#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/core/options.hpp"

#include "basic_block.hpp"

#include <memory_resource>

namespace armajitto::ir {

class Optimizer {
public:
    Optimizer(const Context &context, Options::Optimizer &options, std::pmr::memory_resource &pmrBuffer)
        : m_context(context)
        , m_options(options)
        , m_pmrBuffer(pmrBuffer) {}

    bool Optimize(BasicBlock &block);

private:
    const Context &m_context;
    Options::Optimizer &m_options;

    std::pmr::memory_resource &m_pmrBuffer;

    bool DoOptimizations(BasicBlock &block);
    void DetectIdleLoops(BasicBlock &block);
};

} // namespace armajitto::ir
