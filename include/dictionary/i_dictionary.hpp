#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

template <typename Key, typename Value>
class IRangeDictionary {
public:
    virtual ~IRangeDictionary() = default;
    virtual std::expected<void, Error> visitRange(
        const Key& lower,
        const Key& upper,
        const std::function<void(const Key&, const Value&)>& visitor
    ) const = 0;
};

template <typename Key, typename Value>
class IDictionary {
public:
    virtual ~IDictionary() = default;

    virtual std::size_t getCount() const noexcept = 0;
    virtual std::optional<std::size_t> getCapacity() const noexcept = 0;
    virtual bool containsKey(const Key& key) const = 0;
    virtual std::optional<Value> find(const Key& key) const = 0;
    virtual std::expected<void, Error> add(const Key& key, const Value& value) = 0;
    virtual bool update(const Key& key, const Value& value) = 0;
    virtual bool modify(const Key& key, const std::function<void(Value&)>& modifier) = 0;
    virtual std::unique_ptr<IDictionary<Key, Value>> clone() const = 0;
    virtual std::unique_ptr<IDictionary<Key, Value>> createEmpty() const = 0;

    virtual bool supportsRangeSearch() const noexcept { return false; }

    virtual const IRangeDictionary<Key, Value>& getRangeSearch() const {
        throw std::logic_error("Range search is not supported by this dictionary");
    }

    virtual bool remove(const Key& key) = 0;
    virtual DynamicArray<Key> getKeys() const = 0;
    virtual DynamicArray<Value> getValues() const = 0;
    virtual void clear() noexcept = 0;
};
