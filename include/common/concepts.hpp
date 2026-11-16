#pragma once

#include <concepts>
#include <cstddef>

template <typename Compare, typename T>
concept StrictComparator = std::default_initializable<Compare> &&
    requires(Compare compare, const T& left, const T& right) {
        { compare(left, right) } -> std::convertible_to<bool>;
    };

template <typename Sequence>
concept IndexableSequence = requires(const Sequence& sequence, std::size_t index) {
    { sequence.getLength() } -> std::convertible_to<std::size_t>;
    sequence.at(index);
};

template <typename Sequence, typename Key, typename Extractor, typename Compare>
concept SearchableSequence = IndexableSequence<Sequence> && StrictComparator<Compare, Key> &&
    requires(const Sequence& sequence, Extractor extractor) {
        { extractor(sequence.at(std::size_t{0})) } -> std::convertible_to<Key>;
    };
