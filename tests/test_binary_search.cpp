#include <gtest/gtest.h>

#include "search/binary_search.hpp"
#include "sequence/array_sorted_sequence.hpp"

TEST(BinarySearchTest, HandlesEmptyAndSingleElement) {
    ArraySortedSequence<int> sequence;
    EXPECT_EQ(lowerBound(sequence, 1), 0U);
    EXPECT_EQ(upperBound(sequence, 1), 0U);
    EXPECT_FALSE(binarySearch(sequence, 1).has_value());
    sequence.add(3);
    EXPECT_EQ(binarySearch(sequence, 3), 0U);
}

TEST(BinarySearchTest, FindsBoundariesAndMissingValues) {
    ArraySortedSequence<int> sequence;
    for (const int value : {1, 3, 3, 3, 5, 7}) sequence.add(value);
    EXPECT_EQ(lowerBound(sequence, 3), 1U);
    EXPECT_EQ(upperBound(sequence, 3), 4U);
    EXPECT_EQ(binarySearch(sequence, 1), 0U);
    EXPECT_EQ(binarySearch(sequence, 5), 4U);
    EXPECT_EQ(binarySearch(sequence, 7), 5U);
    EXPECT_FALSE(binarySearch(sequence, 0).has_value());
    EXPECT_FALSE(binarySearch(sequence, 4).has_value());
    EXPECT_FALSE(binarySearch(sequence, 9).has_value());
    EXPECT_EQ((equalRange(sequence, 3)), (TwoElement<std::size_t, std::size_t>{1, 4}));
}

TEST(BinarySearchTest, InclusiveRangeAndInvalidRange) {
    ArraySortedSequence<int> sequence;
    for (const int value : {1, 2, 3, 4, 5}) sequence.add(value);
    auto result = rangeSearch(sequence, 2, 4);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3U);
    EXPECT_EQ((*result)[0], 1U);
    EXPECT_EQ((*result)[2], 3U);
    EXPECT_TRUE(rangeSearch(sequence, 8, 9)->empty());
    EXPECT_EQ(rangeSearch(sequence, 4, 2).error(), SearchError::InvalidRange);
}
