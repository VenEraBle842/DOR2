#pragma once

#include "common/concepts.hpp"
#include "dictionary/i_dictionary.hpp"
#include "tree/binary_search_tree.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename Key, typename Value>
struct DictionaryEntry {
    Key key;
    std::optional<Value> value;

    DictionaryEntry(Key entryKey, Value entryValue)
        : key(std::move(entryKey)), value(std::move(entryValue)) {}

    explicit DictionaryEntry(Key entryKey) : key(std::move(entryKey)) {}
};

template <typename Key, typename Value, typename Compare>
struct DictionaryEntryPointerCompare {
    Compare compare{};

    bool operator()(
        const std::shared_ptr<DictionaryEntry<Key, Value>>& left,
        const std::shared_ptr<DictionaryEntry<Key, Value>>& right
    ) const {
        return compare(left->key, right->key);
    }
};

template <typename Key, typename Value, typename Compare = std::less<Key>>
requires StrictComparator<Compare, Key>
class TreeDictionary final : public IDictionary<Key, Value> {
private:
    using Entry = DictionaryEntry<Key, Value>;
    using EntryPointer = std::shared_ptr<Entry>;
    using EntryCompare = DictionaryEntryPointerCompare<Key, Value, Compare>;

    BinarySearchTree<EntryPointer, EntryCompare> tree_;

    static EntryPointer makeProbe(const Key& key) {
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

    bool containsKey(const Key& key) const override {
        return tree_.contains(makeProbe(key));
    }

    std::optional<Value> find(const Key& key) const override {
        try {
            const EntryPointer& entry = tree_.search(makeProbe(key));
            return entry->value.value();
        } catch (const NodeNotFoundException&) {
            return std::nullopt;
        }
    }

    std::expected<void, Error> add(const Key& key, const Value& value) override {
        if (!tree_.insert(std::make_shared<Entry>(key, value))) {
            return std::unexpected(makeError(ErrorCode::DuplicateKey));
        }
        return {};
    }

    bool update(const Key& key, const Value& value) override {
        return modify(key, [&value](Value& current) { current = value; });
    }

    std::unique_ptr<IDictionary<Key, Value>> createEmpty() const override {
        return std::make_unique<TreeDictionary>();
    }

    std::unique_ptr<IDictionary<Key, Value>> clone() const override {
        return std::make_unique<TreeDictionary>(*this);
    }

    bool modify(const Key& key, const std::function<void(Value&)>& modifier) override {
        try {
            const EntryPointer& entry = tree_.search(makeProbe(key));
            modifier(entry->value.value());
            return true;
        } catch (const NodeNotFoundException&) {
            return false;
        }
    }

    bool remove(const Key& key) override { return tree_.remove(makeProbe(key)); }

    DynamicArray<Key> getKeys() const override {
        DynamicArray<Key> result;
        tree_.forEach(TraversalOrder::LKP, [&result](const EntryPointer& entry) {
            result.push_back(entry->key);
        });
        return result;
    }

    DynamicArray<Value> getValues() const override {
        DynamicArray<Value> result;
        tree_.forEach(TraversalOrder::LKP, [&result](const EntryPointer& entry) {
            result.push_back(entry->value.value());
        });
        return result;
    }

    void clear() noexcept override { tree_.clear(); }
};
