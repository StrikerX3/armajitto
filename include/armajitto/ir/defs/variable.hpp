#pragma once

namespace armajitto::ir {

struct Variable {
    static constexpr size_t kInvalidIndex = ~0;

    Variable()
        : index(kInvalidIndex) {}

    explicit Variable(size_t index)
        : index(index) {}

    size_t Index() const {
        return index;
    }

    bool IsPresent() const {
        return index != kInvalidIndex;
    }

    bool operator==(const Variable &) const = default;

private:
    size_t index;

    friend class Emitter;
};

} // namespace armajitto::ir
