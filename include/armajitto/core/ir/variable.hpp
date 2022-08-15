#pragma once

namespace armajitto::ir {

struct Variable {
    const size_t index;
    const char *name;

    Variable(size_t index, const char *name)
        : index(index)
        , name(name) {}
};

} // namespace armajitto::ir
