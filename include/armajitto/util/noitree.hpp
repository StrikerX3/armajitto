#pragma once

#include <concepts>
#include <map>
#include <optional>
#include <type_traits>

namespace util {

namespace detail {

template <typename T>
struct DefaultValue;

template <typename T>
struct DefaultValue<T*> {
	static constexpr T* value = nullptr;
};

template <typename T>
struct DefaultValue {
	static constexpr T value = T{};
};

template <typename T>
constexpr T defaultValue = DefaultValue<T>::value;

} // namespace detail

/* Non-Overlapping Interval Tree
 *
 * Conceptually, this acts like an array of arbitrary size where interval insertions happen as if inserting a value
 * across all indices of the interval, which results in newer intervals replacing older values at those intervals.
 */
template <std::integral Key, typename Value>
class NonOverlappingIntervalTree {
	static constexpr Value kDefaultValue = detail::defaultValue<Value>;

public:
	void Insert(Key pos, Value value) {
		Insert(pos, pos, value);
	}

	void Insert(Key begin, Key end, Value value) {
		auto beginEntry = m_map.lower_bound(begin);
		if (beginEntry == m_map.end()) {
			// New interval is higher than all existing intervals
			m_map.insert({ end, {begin, value} });
		}
		else {
			auto beginLowerBound = beginEntry->second.lowerBound;
			auto beginUpperBound = beginEntry->first;
			if (end < beginLowerBound) {
				// New interval is lower than all existing intervals
				m_map.insert({ end, {begin, value} });
			}
			else {
				// New interval overlaps at least one existing interval
				auto endEntry = m_map.lower_bound(end);
				if (endEntry == m_map.end()) {
					endEntry--;
				}
				while (endEntry->second.lowerBound > end) {
					endEntry--;
				}
				if (endEntry == beginEntry) {
					// New interval overlaps only one existing interval
					if (end < beginUpperBound) {
						// Shrink existing interval and insert new interval
						beginEntry->second.lowerBound = end + 1;
						m_map.insert({ end, {begin, value} });
						if (begin > beginLowerBound) {
							// Insert old value at the lower portion of the existing interval
							m_map.insert({ begin - 1, {beginLowerBound, beginEntry->second.value} });
						}
					}
					else if (end > beginUpperBound) {
						// Delete old interval and insert new interval
						auto oldValue = beginEntry->second;
						m_map.erase(beginEntry);
						m_map.insert({ end, {begin, value} });
						if (begin > beginLowerBound) {
							m_map.insert({ begin - 1, oldValue });
						}
					}
					else {
						// Replace and resize existing interval
						auto oldValue = beginEntry->second;
						beginEntry->second.lowerBound = begin;
						beginEntry->second.value = value;
						if (begin > beginLowerBound) {
							// Reinsert old value at the lower portion of the old interval
							m_map.insert({ begin - 1, oldValue });
						}
					}
				}
				else {
					// New interval overlaps multiple existing intervals
					auto endUpperBound = endEntry->first;

					// Delete everything between the lower and upper intervals as they're overlapped by the new interval
					m_map.erase(std::next(beginEntry), endEntry);

					if (begin <= beginLowerBound) {
						if (end > endUpperBound) {
							// Delete both existing intervals and insert new interval
							m_map.erase(beginEntry);
							m_map.erase(endEntry);
							m_map.insert({ end, {begin, value} });
						}
						else if (end < endUpperBound) {
							// Shrink existing upper interval, delete lower interval and insert new interval
							endEntry->second.lowerBound = end + 1;
							m_map.erase(beginEntry);
							m_map.insert({ end, {begin, value} });
						}
						else {
							// Delete lower interval and replace upper interval
							m_map.erase(beginEntry);
							endEntry->second.lowerBound = begin;
							endEntry->second.value = value;
						}
					}
					else {
						// Relocate lower interval
						auto oldLowerValue = beginEntry->second;
						m_map.erase(beginEntry);
						m_map.insert({ begin - 1, oldLowerValue });

						if (end > endUpperBound) {
							// Delete upper interval and insert new interval
							m_map.erase(endEntry);
							m_map.insert({ end, {begin, value} });
						}
						else if (end < endUpperBound) {
							// Shrink upper interval and insert new interval
							endEntry->second.lowerBound = end + 1;
							m_map.insert({ end, {begin, value} });
						}
						else {
							// Modify and expand upper interval
							endEntry->second.lowerBound = begin;
							endEntry->second.value = value;
						}
					}
				}
			}
		}
		Merge(m_map.lower_bound(begin));
	}

	void Remove(Key pos) {
		Remove(pos, pos);
	}

	void Remove(Key begin, Key end) {
		auto beginEntry = m_map.lower_bound(begin);
		if (beginEntry == m_map.end()) {
			// Interval is higher than all existing intervals
			return;
		}
		else {
			auto beginLowerBound = beginEntry->second.lowerBound;
			auto beginUpperBound = beginEntry->first;
			if (end < beginLowerBound) {
				// Interval is lower than all existing intervals
				return;
			}
			else {
				// Interval overlaps at least one existing interval
				auto endEntry = m_map.lower_bound(end);
				if (endEntry == m_map.end()) {
					endEntry--;
				}
				while (endEntry->second.lowerBound > end) {
					endEntry--;
				}
				if (endEntry == beginEntry) {
					// New interval overlaps only one existing interval
					if (end < beginUpperBound) {
						// Shrink existing interval
						beginEntry->second.lowerBound = end + 1;
						if (begin > beginLowerBound) {
							// Insert old value at the lower portion of the existing interval
							m_map.insert({ begin - 1, {beginLowerBound, beginEntry->second.value} });
						}
					}
					else if (end > beginUpperBound) {
						// Delete old interval
						auto oldValue = beginEntry->second;
						m_map.erase(beginEntry);
						if (begin > beginLowerBound) {
							m_map.insert({ begin - 1, oldValue });
						}
					}
					else {
						// Replace and resize existing interval
						auto oldValue = beginEntry->second;
						m_map.erase(beginEntry);
						if (begin > beginLowerBound) {
							// Reinsert old value at the lower portion of the old interval
							m_map.insert({ begin - 1, oldValue });
						}
					}
				}
				else {
					// New interval overlaps multiple existing intervals
					auto endUpperBound = endEntry->first;

					// Delete everything between the lower and upper intervals as they're overlapped by the new interval
					m_map.erase(std::next(beginEntry), endEntry);

					if (begin <= beginLowerBound) {
						if (end >= endUpperBound) {
							// Delete both existing intervals
							m_map.erase(beginEntry);
							m_map.erase(endEntry);
						}
						else {
							// Shrink existing upper interval, delete lower interva
							endEntry->second.lowerBound = end + 1;
							m_map.erase(beginEntry);
						}
					}
					else {
						// Relocate lower interval
						auto oldLowerValue = beginEntry->second;
						m_map.erase(beginEntry);
						m_map.insert({ begin - 1, oldLowerValue });

						if (end >= endUpperBound) {
							// Delete upper interval
							m_map.erase(endEntry);
						}
						else {
							// Shrink upper interval
							endEntry->second.lowerBound = end + 1;
						}
					}
				}
			}
		}
	}

	void Clear() {
		m_map.clear();
	}

	bool Contains(Key key) const {
		auto entry = m_map.lower_bound(key);
		return (entry != m_map.end()) && (key >= entry->second.lowerBound);
	}

	Value At(Key key) const {
		auto entry = m_map.lower_bound(key);
		if (entry != m_map.end() && (key >= entry->second.lowerBound)) {
			return entry->second.value;
		}
		else {
			return kDefaultValue;
		}
	}

	std::optional<std::pair<Key, Key>> LowerBound(Key key) const {
		auto entry = m_map.lower_bound(key);
		if (entry != m_map.end()) {
			return std::make_pair(entry->second.lowerBound, entry->first);
		}
		else {
			return std::nullopt;
		}
	}

private:
	struct Entry {
		Key lowerBound;
		Value value;
	};

	std::map<Key, Entry> m_map;

	void Merge(typename std::map<Key, Entry>::iterator it) {
		if (it != m_map.begin()) {
			auto left = it;
			do {
				left = std::prev(it);
				if (left->first != it->second.lowerBound - 1) {
					break;
				}
				if (left->second.value != it->second.value) {
					break;
				}
				it->second.lowerBound = left->second.lowerBound;
				auto oldLeft = left;
				left = std::prev(left);
				m_map.erase(oldLeft);
			} while (left != m_map.end());
		}
		if (it != m_map.end()) {
			for (auto right = std::next(it); right != m_map.end(); right = std::next(right)) {
				if (right->second.lowerBound != it->first + 1) {
					break;
				}
				if (right->second.value != it->second.value) {
					break;
				}
				right->second.lowerBound = it->second.lowerBound;
				auto oldIt = it;
				it = std::next(right);
				m_map.erase(oldIt);
			}
		}
	}
};

} // namespace util
