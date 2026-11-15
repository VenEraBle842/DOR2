#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <expected>
#include <optional>

template <typename TKey, typename TValue>
class IDictionary {
public:
    virtual ~IDictionary() = default;

    virtual std::size_t getCount() const noexcept = 0;
    virtual std::optional<std::size_t> getCapacity() const noexcept = 0;
    virtual bool containsKey(const TKey& key) const = 0;
    virtual std::optional<TValue> find(const TKey& key) const = 0;
    virtual std::expected<void, DictionaryError> add(const TKey& key, const TValue& value) = 0;
    virtual bool update(const TKey& key, const TValue& value) = 0;
    virtual bool remove(const TKey& key) = 0;
    virtual DynamicArray<TKey> getKeys() const = 0;
    virtual DynamicArray<TValue> getValues() const = 0;
    virtual void clear() noexcept = 0;
};
