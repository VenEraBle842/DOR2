#pragma once

#include <cstdint>
#include <functional>

using RecordId = std::uint64_t;

template <typename TKey>
struct IndexEntry {
    TKey key;
    RecordId recordId{0};

    friend bool operator==(const IndexEntry&, const IndexEntry&) = default;
};

template <typename TKey, typename KeyCompare = std::less<TKey>>
struct IndexEntryCompare {
    KeyCompare compare{};

    bool operator()(const IndexEntry<TKey>& left, const IndexEntry<TKey>& right) const {
        if (compare(left.key, right.key)) return true;
        if (compare(right.key, left.key)) return false;
        return left.recordId < right.recordId;
    }
};

template <typename TKey>
struct IndexKeyExtractor {
    const TKey& operator()(const IndexEntry<TKey>& entry) const noexcept { return entry.key; }
};
