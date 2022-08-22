#pragma once

#include <format>
#include <string>

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

    std::string ToString() const {
        if (IsPresent()) {
            return std::format("$v{}", index);
        } else {
            return std::string("$v?");
        }
    }

    bool operator==(const Variable &) const = default;

private:
    size_t index;

    friend class Emitter;
};

} // namespace armajitto::ir
