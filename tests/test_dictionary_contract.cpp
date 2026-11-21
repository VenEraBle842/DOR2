#include <gtest/gtest.h>

#include "dictionary/tree_dictionary.hpp"
#include "dictionary/sorted_array_dictionary.hpp"

#include <string>

namespace {

void expectDictionaryMutations(IDictionary<int, std::string>& dictionary) {
    EXPECT_FALSE(dictionary.find(3));
    EXPECT_FALSE(dictionary.update(3, "missing"));
    EXPECT_FALSE(dictionary.modify(3, [](auto&) { FAIL() << "Missing key invoked modifier"; }));
    for (int key : {3, 1, 5, 2, 4}) {
        ASSERT_TRUE(dictionary.add(key, std::to_string(key)));
    }
    EXPECT_EQ(dictionary.add(3, "duplicate").error().code, ErrorCode::DuplicateKey);
    ASSERT_TRUE(dictionary.update(3, "updated"));
    ASSERT_TRUE(dictionary.modify(3, [](auto& value) { value += "!"; }));
    EXPECT_EQ(dictionary.find(3), "updated!");
    EXPECT_TRUE(dictionary.remove(1));
    EXPECT_TRUE(dictionary.remove(5));
    EXPECT_TRUE(dictionary.remove(3));
    EXPECT_FALSE(dictionary.remove(3));
    const auto keys = dictionary.getKeys();
    const auto values = dictionary.getValues();
    ASSERT_EQ(keys.size(), 2U);
    EXPECT_EQ(keys[0], 2);
    EXPECT_EQ(keys[1], 4);
    EXPECT_EQ(values[0], "2");
    EXPECT_EQ(values[1], "4");
    dictionary.clear();
    EXPECT_EQ(dictionary.getCount(), 0U);
    ASSERT_TRUE(dictionary.add(9, "reuse"));
    EXPECT_EQ(dictionary.find(9), "reuse");
}

template <typename Dictionary>
void expectDictionaryCopies() {
    Dictionary dictionary;
    ASSERT_TRUE(dictionary.add(1, "original"));
    auto clone = dictionary.clone();
    ASSERT_TRUE(clone->modify(1, [](auto& value) { value = "copy"; }));
    EXPECT_EQ(dictionary.find(1), "original");
    auto empty = dictionary.createEmpty();
    EXPECT_EQ(empty->getCount(), 0U);
    ASSERT_TRUE(empty->add(1, "fresh"));
    EXPECT_EQ(clone->find(1), "copy");
    Dictionary moved(std::move(dictionary));
    EXPECT_EQ(moved.find(1), "original");
    EXPECT_EQ(dictionary.getCount(), 0U);
    ASSERT_TRUE(dictionary.add(2, "reused after move"));
    EXPECT_FALSE(moved.find(2));
}

} // namespace

TEST(DictionaryContractTest, TreeMutationsAndEnumeration) {
    TreeDictionary<int, std::string> dictionary;
    expectDictionaryMutations(dictionary);
}

TEST(DictionaryContractTest, SortedArrayMutationsAndEnumeration) {
    SortedArrayDictionary<int, std::string> dictionary;
    expectDictionaryMutations(dictionary);
}

TEST(DictionaryContractTest, TreeCloneAndMoveOwnIndependentValues) {
    expectDictionaryCopies<TreeDictionary<int, std::string>>();
}

TEST(DictionaryContractTest, SortedArrayCloneAndMoveOwnIndependentValues) {
    expectDictionaryCopies<SortedArrayDictionary<int, std::string>>();
}

TEST(SortedArrayDictionaryTest, InclusiveRangesFollowCustomComparatorAndSkipOtherKeys) {
    SortedArrayDictionary<int, int, std::greater<int>> dictionary;
    ASSERT_TRUE(dictionary.supportsRangeSearch());
    const auto& range = dictionary.getRangeSearch();
    std::size_t visits = 0;
    ASSERT_TRUE(range.visitRange(4, 2, [&](int, int) { ++visits; }));
    EXPECT_EQ(visits, 0U);
    for (int key = 0; key < 7; ++key) {
        ASSERT_TRUE(dictionary.add(key, key * 10));
    }
    DynamicArray<int> keys;
    ASSERT_TRUE(range.visitRange(4, 2, [&](int key, int value) {
        keys.push_back(key);
        EXPECT_EQ(value, key * 10);
    }));
    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], 4);
    EXPECT_EQ(keys[1], 3);
    EXPECT_EQ(keys[2], 2);
    EXPECT_EQ(range.visitRange(2, 4, [](int, int) {}).error().code, ErrorCode::InvalidRange);
    keys.clear();
    ASSERT_TRUE(range.visitRange(3, 3, [&](int key, int) { keys.push_back(key); }));
    ASSERT_EQ(keys.size(), 1U);
    EXPECT_EQ(keys[0], 3);
    TreeDictionary<int, int> tree;
    EXPECT_FALSE(tree.supportsRangeSearch());
    EXPECT_THROW(tree.getRangeSearch(), std::logic_error);
}

TEST(SortedArrayDictionaryTest, EquivalentKeysNeedNoEqualityOperator) {
    struct Key {
        int value{0};
    };

    struct Compare {
        bool operator()(const Key& left, const Key& right) const {
            return left.value < right.value;
        }
    };

    struct Value {
        int value{0};

        Value() = default;
        explicit Value(int initialValue) : value(initialValue) {}
    };
    SortedArrayDictionary<Key, Value, Compare> dictionary;
    ASSERT_TRUE(dictionary.add({2}, Value{10}));
    EXPECT_EQ(dictionary.add({2}, Value{20}).error().code, ErrorCode::DuplicateKey);
    ASSERT_TRUE(dictionary.update({2}, Value{30}));
    EXPECT_EQ(dictionary.find({2})->value, 30);
    ASSERT_TRUE(dictionary.remove({2}));
    EXPECT_FALSE(dictionary.find({2}));
}
