#pragma once

#include "common/concepts.hpp"
#include "containers/dynamic_array.hpp"
#include "search/binary_search.hpp"
#include "sequence/i_sorted_sequence.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

template <typename T, typename Compare = std::less<T>>
requires StrictComparator<Compare, T>
class ArraySortedSequence final : public ISortedSequence<T> {
private:
    DynamicArray<T> items_;
    Compare compare_{};

public:
    ArraySortedSequence() = default;
    ArraySortedSequence(const ArraySortedSequence&) = default;
    ArraySortedSequence& operator=(const ArraySortedSequence&) = default;
    ArraySortedSequence(ArraySortedSequence&&) noexcept = default;
    ArraySortedSequence& operator=(ArraySortedSequence&&) noexcept = default;
    ~ArraySortedSequence() override = default;

    std::size_t getLength() const noexcept override { return items_.size(); }
    bool getIsEmpty() const noexcept override { return items_.empty(); }

    std::optional<T> get(std::size_t index) const override {
        if (index >= items_.size()) return std::nullopt;
        return items_[index];
    }

    const T& at(std::size_t index) const override { return items_.at(index); }

    std::optional<T> getFirst() const override { return get(0); }

    std::optional<T> getLast() const override {
        return items_.empty() ? std::nullopt : get(items_.size() - 1);
    }

    std::optional<std::size_t> indexOf(const T& element) const override {
        return binarySearch(*this, element, IdentityExtractor{}, compare_);
    }

    std::expected<std::unique_ptr<ISortedSequence<T>>, SequenceError>
    getSubsequence(std::size_t startIndex, std::size_t endIndex) const override {
        if (startIndex > endIndex || endIndex >= items_.size()) {
            return std::unexpected(SequenceError::InvalidRange);
        }
        auto result = std::make_unique<ArraySortedSequence<T, Compare>>();
        for (std::size_t index = startIndex; index <= endIndex; ++index) {
            result->add(items_[index]);
        }
        return std::unique_ptr<ISortedSequence<T>>(std::move(result));
    }

    void add(const T& element) override {
        T value = element;
        const std::size_t position = upperBound(*this, value, IdentityExtractor{}, compare_);
        items_.push_back(value);
        for (std::size_t index = items_.size() - 1; index > position; --index) {
            items_[index] = std::move(items_[index - 1]);
        }
        items_[position] = std::move(value);
    }

    void clear() noexcept override { items_.clear(); }
};
