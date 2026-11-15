#include <gtest/gtest.h>

#include "dictionary/tree_dictionary.hpp"

#include <functional>
#include <utility>

TEST(TreeDictionaryTest, EmptyAndCapacity) {
    TreeDictionary<int, int> dictionary;
    EXPECT_EQ(dictionary.getCount(), 0U);
    EXPECT_FALSE(dictionary.getCapacity().has_value());
    EXPECT_FALSE(dictionary.find(1).has_value());
    EXPECT_FALSE(dictionary.remove(1));
}

TEST(TreeDictionaryTest, AddFindDuplicateAndOrder) {
    TreeDictionary<int, int> dictionary;
    EXPECT_TRUE(dictionary.add(4, 40).has_value());
    EXPECT_TRUE(dictionary.add(2, 20).has_value());
    EXPECT_TRUE(dictionary.add(6, 60).has_value());
    EXPECT_FALSE(dictionary.add(2, 99).has_value());
    ASSERT_TRUE(dictionary.find(4).has_value());
    EXPECT_TRUE(dictionary.update(4, 41));
    EXPECT_EQ(dictionary.find(4).value(), 41);
    EXPECT_FALSE(dictionary.update(99, 1));

    const auto keys = dictionary.getKeys();
    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], 2);
    EXPECT_EQ(keys[1], 4);
    EXPECT_EQ(keys[2], 6);
}

TEST(TreeDictionaryTest, RemovesEveryNodeShape) {
    TreeDictionary<int, int> dictionary;
    for (const int value : {5, 3, 8, 2, 4, 7, 9, 6}) {
        ASSERT_TRUE(dictionary.add(value, value).has_value());
    }
    EXPECT_TRUE(dictionary.remove(2));
    EXPECT_TRUE(dictionary.remove(7));
    EXPECT_TRUE(dictionary.remove(5));
    EXPECT_FALSE(dictionary.containsKey(5));
    EXPECT_EQ(dictionary.getCount(), 5U);
}

TEST(TreeDictionaryTest, CopyIsIndependentAndMoveEmptiesSource) {
    TreeDictionary<int, int> original;
    ASSERT_TRUE(original.add(1, 10).has_value());
    TreeDictionary<int, int> copy = original;
    ASSERT_TRUE(copy.update(1, 20));
    EXPECT_EQ(original.find(1).value(), 10);

    TreeDictionary<int, int> moved = std::move(copy);
    EXPECT_EQ(moved.find(1).value(), 20);
    EXPECT_EQ(copy.getCount(), 0U);
}

TEST(TreeDictionaryTest, SupportsCustomComparatorAndDegenerateInput) {
    TreeDictionary<int, int, std::greater<int>> reverse;
    for (int value = 0; value < 200; ++value) {
        ASSERT_TRUE(reverse.add(value, value).has_value());
    }
    const auto keys = reverse.getKeys();
    ASSERT_EQ(keys.size(), 200U);
    EXPECT_EQ(keys[0], 199);
    EXPECT_EQ(keys[199], 0);
}

TEST(TreeDictionaryTest, HandlesBothDegenerateOrders) {
    TreeDictionary<int, int> ascending;
    TreeDictionary<int, int> descending;
    for (int value = 0; value < 150; ++value) {
        ASSERT_TRUE(ascending.add(value, value).has_value());
        ASSERT_TRUE(descending.add(149 - value, value).has_value());
    }
    EXPECT_EQ(ascending.getCount(), 150U);
    EXPECT_EQ(descending.getCount(), 150U);
    EXPECT_EQ(ascending.find(149).value(), 149);
    EXPECT_EQ(descending.find(0).value(), 149);
}
