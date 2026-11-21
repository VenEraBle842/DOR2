#pragma once

#include "dictionary/i_dictionary.hpp"
#include "sequence/array_sorted_sequence.hpp"

template <typename Key, typename Value>
struct SortedDictionaryEntry {
    Key key;
    Value value;
};

template <typename Key, typename Value, typename Compare>
struct SortedDictionaryEntryCompare {
    Compare compare{};

    bool operator()(
        const SortedDictionaryEntry<Key, Value>& left,
        const SortedDictionaryEntry<Key, Value>& right
    ) const {
        return compare(left.key, right.key);
    }
};

template <typename Key, typename Value, typename Compare = std::less<Key>>
requires StrictComparator<Compare, Key>
class SortedArrayDictionary final
    : public IDictionary<Key, Value>, public IRangeDictionary<Key, Value> {
private:
    using Entry = SortedDictionaryEntry<Key, Value>;
    using EntryCompare = SortedDictionaryEntryCompare<Key, Value, Compare>;

    struct ExtractKey {
        const Key& operator()(const Entry& entry) const noexcept { return entry.key; }
    };

    ArraySortedSequence<Entry, EntryCompare> entries_;

    std::optional<std::size_t> positionOf(const Key& key) const {
        return binarySearch(entries_, key, ExtractKey{}, Compare{});
    }

public:
    SortedArrayDictionary() = default;
    SortedArrayDictionary(const SortedArrayDictionary&) = default;
    SortedArrayDictionary& operator=(const SortedArrayDictionary&) = default;
    SortedArrayDictionary(SortedArrayDictionary&&) noexcept = default;
    SortedArrayDictionary& operator=(SortedArrayDictionary&&) noexcept = default;
    ~SortedArrayDictionary() override = default;

    std::size_t getCount() const noexcept override { return entries_.getLength(); }
    std::optional<std::size_t> getCapacity() const noexcept override { return entries_.getCapacity(); }

    bool containsKey(const Key& key) const override {
        return positionOf(key).has_value();
    }

    std::optional<Value> find(const Key& key) const override {
        const auto position = positionOf(key);
        if (!position) return std::nullopt;
        return entries_.at(*position).value;
    }

    std::expected<void, Error> add(const Key& key, const Value& value) override {
        if (positionOf(key)) return std::unexpected(makeError(ErrorCode::DuplicateKey));
        entries_.add({key, value});
        return {};
    }

    bool update(const Key& key, const Value& value) override {
        return modify(key, [&value](Value& current) { current = value; });
    }

    bool modify(const Key& key, const std::function<void(Value&)>& modifier) override {
        const auto position = positionOf(key);
        if (!position) return false;
        Entry replacement = entries_.at(*position);
        modifier(replacement.value);
        return entries_.replaceEquivalent(*position, replacement);
    }

    bool remove(const Key& key) override {
        const auto position = positionOf(key);
        return position && entries_.removeAt(*position);
    }

    DynamicArray<Key> getKeys() const override {
        DynamicArray<Key> result;
        for (std::size_t index = 0; index < getCount(); ++index) {
            result.push_back(entries_.at(index).key);
        }
        return result;
    }

    DynamicArray<Value> getValues() const override {
        DynamicArray<Value> result;
        for (std::size_t index = 0; index < getCount(); ++index) {
            result.push_back(entries_.at(index).value);
        }
        return result;
    }

    void clear() noexcept override { entries_.clear(); }

    std::unique_ptr<IDictionary<Key, Value>> createEmpty() const override {
        return std::make_unique<SortedArrayDictionary>();
    }

    std::unique_ptr<IDictionary<Key, Value>> clone() const override {
        return std::make_unique<SortedArrayDictionary>(*this);
    }

    bool supportsRangeSearch() const noexcept override { return true; }

    const IRangeDictionary<Key, Value>& getRangeSearch() const noexcept override {
        return *this;
    }

    std::expected<void, Error> visitRange(
        const Key& lower,
        const Key& upper,
        const std::function<void(const Key&, const Value&)>& visitor
    ) const override {
        if (Compare{}(upper, lower)) return std::unexpected(makeError(ErrorCode::InvalidRange));
        const std::size_t begin = lowerBound(entries_, lower, ExtractKey{}, Compare{});
        const std::size_t end = upperBound(entries_, upper, ExtractKey{}, Compare{});
        for (std::size_t position = begin; position < end; ++position) {
            const Entry& entry = entries_.at(position);
            visitor(entry.key, entry.value);
        }
        return {};
    }
};
