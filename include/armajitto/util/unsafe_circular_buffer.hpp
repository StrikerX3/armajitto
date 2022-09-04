#pragma once

#include <array>

namespace util {

template <typename T, size_t capacity>
class CircularBuffer {
public:
    void Push(T item) {
        m_items[Advance(m_tail)] = item;
        assert(m_head != m_tail); // overflow
    }

    T Pop() {
        assert(m_head != m_tail); // underflow
        return m_items[Advance(m_head)];
    }

    bool IsEmpty() {
        return m_head == m_tail;
    }

    constexpr size_t Capacity() const {
        return capacity;
    }

    bool Erase(const T &item) {
        if (m_head == m_tail) {
            return false;
        }
        for (size_t i = m_head; i != m_tail; Advance(i)) {
            if (m_items[i] == item) {
                Retrocede(m_tail);
                std::swap(m_items[i], m_items[m_tail]);
                return true;
            }
        }
        return false;
    }

private:
    std::array<T, capacity> m_items;
    size_t m_head = 0;
    size_t m_tail = 0;

    size_t Advance(size_t &pointer) {
        size_t output = pointer;
        if (++pointer == capacity) {
            pointer = 0;
        }
        return output;
    }

    void Retrocede(size_t &pointer) {
        if (pointer == 0) {
            pointer = capacity - 1;
        } else {
            --pointer;
        }
    }
};

} // namespace util
