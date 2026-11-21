#pragma once

#include "dictionary/i_dictionary.hpp"
#include "sequence/array_sorted_sequence.hpp"

template <typename TKey, typename TValue>
struct SortedDictionaryEntry {
    TKey key;
    TValue value;
};

template <typename TKey, typename TValue, typename Compare>
struct SortedDictionaryEntryCompare {
    Compare compare{};

    bool operator()(
        const SortedDictionaryEntry<TKey, TValue>& left,
        const SortedDictionaryEntry<TKey, TValue>& right
    ) const {
        return compare(left.key, right.key);
    }
};

template <typename TKey, typename TValue, typename Compare = std::less<TKey>>
requires StrictComparator<Compare, TKey>
class SortedArrayDictionary final
    : public IDictionary<TKey, TValue>, public IRangeDictionary<TKey, TValue> {
private:
    using Entry = SortedDictionaryEntry<TKey, TValue>;
    using EntryCompare = SortedDictionaryEntryCompare<TKey, TValue, Compare>;

    struct ExtractKey {
        const TKey& operator()(const Entry& entry) const noexcept { return entry.key; }
    };

    ArraySortedSequence<Entry, EntryCompare> entries_;

    std::optional<std::size_t> positionOf(const TKey& key) const {
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

    bool containsKey(const TKey& key) const override {
        return positionOf(key).has_value();
    }

    std::optional<TValue> find(const TKey& key) const override {
        const auto position = positionOf(key);
        if (!position) return std::nullopt;
        return entries_.at(*position).value;
    }

    std::expected<void, Error> add(const TKey& key, const TValue& value) override {
        if (positionOf(key)) return std::unexpected(makeError(ErrorCode::DuplicateKey));
        entries_.add({key, value});
        return {};
    }

    bool update(const TKey& key, const TValue& value) override {
        return modify(key, [&value](TValue& current) { current = value; });
    }

    bool modify(const TKey& key, const std::function<void(TValue&)>& modifier) override {
        const auto position = positionOf(key);
        if (!position) return false;
        Entry replacement = entries_.at(*position);
        modifier(replacement.value);
        return entries_.replaceEquivalent(*position, replacement);
    }

    bool remove(const TKey& key) override {
        const auto position = positionOf(key);
        return position && entries_.removeAt(*position);
    }

    DynamicArray<TKey> getKeys() const override {
        DynamicArray<TKey> result;
        for (std::size_t index = 0; index < getCount(); ++index) {
            result.push_back(entries_.at(index).key);
        }
        return result;
    }

    DynamicArray<TValue> getValues() const override {
        DynamicArray<TValue> result;
        for (std::size_t index = 0; index < getCount(); ++index) {
            result.push_back(entries_.at(index).value);
        }
        return result;
    }

    void clear() noexcept override { entries_.clear(); }

    std::unique_ptr<IDictionary<TKey, TValue>> createEmpty() const override {
        return std::make_unique<SortedArrayDictionary>();
    }

    std::unique_ptr<IDictionary<TKey, TValue>> clone() const override {
        return std::make_unique<SortedArrayDictionary>(*this);
    }

    bool supportsRangeSearch() const noexcept override { return true; }

    const IRangeDictionary<TKey, TValue>& rangeSearch() const noexcept override {
        return *this;
    }

    std::expected<void, Error> visitRange(
        const TKey& lower,
        const TKey& upper,
        const std::function<void(const TKey&, const TValue&)>& visitor
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
