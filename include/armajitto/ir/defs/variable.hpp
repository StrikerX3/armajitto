#pragma once

namespace armajitto::ir {

struct Variable {
    static constexpr size_t kInvalidIndex = ~0;

    Variable()
        : index(kInvalidIndex) {}

    size_t Index() const {
        return index;
    }

    bool IsPresent() const {
        return index != kInvalidIndex;
    }

private:
    explicit Variable(size_t index)
        : index(index) {}

    size_t index;

    friend class Emitter;
};

} // namespace armajitto::ir
