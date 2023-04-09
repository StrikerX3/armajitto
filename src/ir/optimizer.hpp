#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/core/options.hpp"

#include "basic_block.hpp"
#include "core/memory_map_priv_access.hpp"

#include <memory_resource>

namespace armajitto::ir {

class Optimizer {
public:
    Optimizer(Context &context, Options::Optimizer &options, std::pmr::memory_resource &pmrBuffer)
        : m_options(options)
        , m_memMap(context.GetSystem().GetMemoryMap())
        , m_pmrBuffer(pmrBuffer) {}

    bool Optimize(BasicBlock &block);

private:
    Options::Optimizer &m_options;
    MemoryMapPrivateAccess m_memMap;

    std::pmr::memory_resource &m_pmrBuffer;
};

} // namespace armajitto::ir
