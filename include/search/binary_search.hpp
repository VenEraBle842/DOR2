#pragma once

#include "common/concepts.hpp"
#include "common/errors.hpp"
#include "common/two_element.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>

struct IdentityExtractor {
    template <typename T>
    const T& operator()(const T& value) const noexcept { return value; }
};

template <typename Sequence, typename Key, typename Extractor = IdentityExtractor,
          typename Compare = std::less<Key>>
requires SearchableSequence<Sequence, Key, Extractor, Compare>
std::size_t lowerBound(
    const Sequence& sequence,
    const Key& key,
    Extractor extractor = {},
    Compare compare = {}
) {
    std::size_t first = 0;
    std::size_t count = sequence.getLength();
    while (count > 0) {
        const std::size_t step = count / 2;
        const std::size_t position = first + step;
        if (compare(extractor(sequence.at(position)), key)) {
            first = position + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename Sequence, typename Key, typename Extractor = IdentityExtractor,
          typename Compare = std::less<Key>>
requires SearchableSequence<Sequence, Key, Extractor, Compare>
std::size_t upperBound(
    const Sequence& sequence,
    const Key& key,
    Extractor extractor = {},
    Compare compare = {}
) {
    std::size_t first = 0;
    std::size_t count = sequence.getLength();
    while (count > 0) {
        const std::size_t step = count / 2;
        const std::size_t position = first + step;
        if (!compare(key, extractor(sequence.at(position)))) {
            first = position + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename Sequence, typename Key, typename Extractor = IdentityExtractor,
          typename Compare = std::less<Key>>
requires SearchableSequence<Sequence, Key, Extractor, Compare>
std::optional<std::size_t> binarySearch(
    const Sequence& sequence,
    const Key& key,
    Extractor extractor = {},
    Compare compare = {}
) {
    const std::size_t position = lowerBound(sequence, key, extractor, compare);
    if (position == sequence.getLength()) return std::nullopt;
    const auto value = extractor(sequence.at(position));
    if (compare(key, value) || compare(value, key)) return std::nullopt;
    return position;
}

template <typename Sequence, typename Key, typename Extractor = IdentityExtractor,
          typename Compare = std::less<Key>>
requires SearchableSequence<Sequence, Key, Extractor, Compare>
TwoElement<std::size_t, std::size_t> equalRange(
    const Sequence& sequence,
    const Key& key,
    Extractor extractor = {},
    Compare compare = {}
) {
    return {lowerBound(sequence, key, extractor, compare),
            upperBound(sequence, key, extractor, compare)};
}

template <typename Sequence, typename Key, typename Extractor = IdentityExtractor,
          typename Compare = std::less<Key>>
requires SearchableSequence<Sequence, Key, Extractor, Compare>
std::expected<DynamicArray<std::size_t>, SearchError> rangeSearch(
    const Sequence& sequence,
    const Key& lower,
    const Key& upper,
    Extractor extractor = {},
    Compare compare = {}
) {
    if (compare(upper, lower)) return std::unexpected(SearchError::InvalidRange);

    DynamicArray<std::size_t> positions;
    const std::size_t begin = lowerBound(sequence, lower, extractor, compare);
    const std::size_t end = upperBound(sequence, upper, extractor, compare);
    for (std::size_t position = begin; position < end; ++position) {
        positions.push_back(position);
    }
    return positions;
}
