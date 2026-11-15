#pragma once

#include <concepts>
#include <cstddef>
#include <istream>
#include <ostream>

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

template <typename Extractor, typename Element, typename Key>
concept KeyExtractor = requires(Extractor extractor, const Element& element) {
    { extractor(element) } -> std::convertible_to<Key>;
};

template <typename Sequence, typename Key, typename Extractor, typename Compare>
concept SearchableSequence = IndexableSequence<Sequence> && StrictComparator<Compare, Key> &&
    requires(const Sequence& sequence, Extractor extractor) {
        { extractor(sequence.at(std::size_t{0})) } -> std::convertible_to<Key>;
    };

template <typename T>
concept StreamRecord = std::default_initializable<T> && requires(
    std::ostream& output,
    std::istream& input,
    const T& constant,
    T& value
) {
    { output << constant } -> std::same_as<std::ostream&>;
    { input >> value } -> std::same_as<std::istream&>;
};
