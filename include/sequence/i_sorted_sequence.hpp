#pragma once

#include "common/errors.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

template <typename T>
class ISortedSequence {
public:
    virtual ~ISortedSequence() = default;

    virtual std::size_t getLength() const noexcept = 0;
    virtual bool isEmpty() const noexcept = 0;
    virtual std::optional<T> get(std::size_t index) const = 0;
    virtual const T& at(std::size_t index) const = 0;
    virtual std::optional<T> getFirst() const = 0;
    virtual std::optional<T> getLast() const = 0;
    virtual std::optional<std::size_t> indexOf(const T& element) const = 0;
    virtual std::expected<std::unique_ptr<ISortedSequence<T>>, SequenceError>
        getSubsequence(std::size_t startIndex, std::size_t endIndex) const = 0;
    virtual void add(const T& element) = 0;
    virtual void clear() noexcept = 0;
};
