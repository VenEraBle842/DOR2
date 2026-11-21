#pragma once

#include "common/concepts.hpp"
#include "dictionary/i_dictionary.hpp"
#include "tree/binary_search_tree.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename TKey, typename TValue>
struct DictionaryEntry {
    TKey key;
    std::optional<TValue> value;

    DictionaryEntry(TKey entryKey, TValue entryValue)
        : key(std::move(entryKey)), value(std::move(entryValue)) {}

    explicit DictionaryEntry(TKey entryKey) : key(std::move(entryKey)) {}
};

template <typename TKey, typename TValue, typename Compare>
struct DictionaryEntryPointerCompare {
    Compare compare{};

    bool operator()(
        const std::shared_ptr<DictionaryEntry<TKey, TValue>>& left,
        const std::shared_ptr<DictionaryEntry<TKey, TValue>>& right
    ) const {
        return compare(left->key, right->key);
    }
};

template <typename TKey, typename TValue, typename Compare = std::less<TKey>>
requires StrictComparator<Compare, TKey>
class TreeDictionary final : public IDictionary<TKey, TValue> {
private:
    using Entry = DictionaryEntry<TKey, TValue>;
    using EntryPointer = std::shared_ptr<Entry>;
    using EntryCompare = DictionaryEntryPointerCompare<TKey, TValue, Compare>;

    BinarySearchTree<EntryPointer, EntryCompare> tree_;

    static EntryPointer makeProbe(const TKey& key) {
        return std::make_shared<Entry>(key);
    }

public:
    TreeDictionary() = default;

    TreeDictionary(const TreeDictionary& other) {
        other.tree_.forEach(TraversalOrder::KLP, [this](const EntryPointer& entry) {
            const auto inserted = add(entry->key, entry->value.value());
            if (!inserted) {
                throw std::logic_error("duplicate key while copying TreeDictionary");
            }
        });
    }

    TreeDictionary& operator=(const TreeDictionary& other) {
        if (this != &other) {
            TreeDictionary copy(other);
            tree_ = std::move(copy.tree_);
        }
        return *this;
    }

    TreeDictionary(TreeDictionary&&) noexcept = default;
    TreeDictionary& operator=(TreeDictionary&&) noexcept = default;
    ~TreeDictionary() override = default;

    std::size_t getCount() const noexcept override { return tree_.size(); }
    std::optional<std::size_t> getCapacity() const noexcept override { return std::nullopt; }

    bool containsKey(const TKey& key) const override {
        return tree_.contains(makeProbe(key));
    }

    std::optional<TValue> find(const TKey& key) const override {
        try {
            const EntryPointer& entry = tree_.search(makeProbe(key));
            return entry->value.value();
        } catch (const NodeNotFoundException&) {
            return std::nullopt;
        }
    }

    std::expected<void, Error> add(const TKey& key, const TValue& value) override {
        if (!tree_.insert(std::make_shared<Entry>(key, value))) {
            return std::unexpected(makeError(ErrorCode::DuplicateKey));
        }
        return {};
    }

    bool update(const TKey& key, const TValue& value) override {
        return modify(key, [&value](TValue& current) { current = value; });
    }

    std::unique_ptr<IDictionary<TKey, TValue>> createEmpty() const override {
        return std::make_unique<TreeDictionary>();
    }

    std::unique_ptr<IDictionary<TKey, TValue>> clone() const override {
        return std::make_unique<TreeDictionary>(*this);
    }

    bool modify(const TKey& key, const std::function<void(TValue&)>& modifier) override {
        try {
            const EntryPointer& entry = tree_.search(makeProbe(key));
            modifier(entry->value.value());
            return true;
        } catch (const NodeNotFoundException&) {
            return false;
        }
    }

    bool remove(const TKey& key) override { return tree_.remove(makeProbe(key)); }

    DynamicArray<TKey> getKeys() const override {
        DynamicArray<TKey> result;
        tree_.forEach(TraversalOrder::LKP, [&result](const EntryPointer& entry) {
            result.push_back(entry->key);
        });
        return result;
    }

    DynamicArray<TValue> getValues() const override {
        DynamicArray<TValue> result;
        tree_.forEach(TraversalOrder::LKP, [&result](const EntryPointer& entry) {
            result.push_back(entry->value.value());
        });
        return result;
    }

    void clear() noexcept override { tree_.clear(); }
};
