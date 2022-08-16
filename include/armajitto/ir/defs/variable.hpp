#pragma once

namespace armajitto::ir {

struct Variable {
    static constexpr size_t kInvalidIndex = ~0;

    const size_t index;
    const char *name;

    Variable()
        : index(kInvalidIndex)
        , name("") {}

    Variable(size_t index, const char *name)
        : index(index)
        , name(name) {}
};

} // namespace armajitto::ir
