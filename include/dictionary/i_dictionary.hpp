#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

template <typename TKey, typename TValue>
class IRangeDictionary {
public:
    virtual ~IRangeDictionary() = default;
    virtual std::expected<void, Error> visitRange(
        const TKey& lower,
        const TKey& upper,
        const std::function<void(const TKey&, const TValue&)>& visitor
    ) const = 0;
};

template <typename TKey, typename TValue>
class IDictionary {
public:
    virtual ~IDictionary() = default;

    virtual std::size_t getCount() const noexcept = 0;
    virtual std::optional<std::size_t> getCapacity() const noexcept = 0;
    virtual bool containsKey(const TKey& key) const = 0;
    virtual std::optional<TValue> find(const TKey& key) const = 0;
    virtual std::expected<void, Error> add(const TKey& key, const TValue& value) = 0;
    virtual bool update(const TKey& key, const TValue& value) = 0;
    virtual bool modify(const TKey& key, const std::function<void(TValue&)>& modifier) = 0;
    virtual std::unique_ptr<IDictionary<TKey, TValue>> clone() const = 0;
    virtual std::unique_ptr<IDictionary<TKey, TValue>> createEmpty() const = 0;

    virtual bool supportsRangeSearch() const noexcept { return false; }

    virtual const IRangeDictionary<TKey, TValue>& rangeSearch() const {
        throw std::logic_error("Range search is not supported by this dictionary");
    }

    virtual bool remove(const TKey& key) = 0;
    virtual DynamicArray<TKey> getKeys() const = 0;
    virtual DynamicArray<TValue> getValues() const = 0;
    virtual void clear() noexcept = 0;
};
