#include <gtest/gtest.h>

#include "sequence/array_sorted_sequence.hpp"

#include <utility>

TEST(ArraySortedSequenceTest, AddsAtEveryPositionAndKeepsDuplicates) {
    ArraySortedSequence<int> sequence;
    sequence.add(4);
    sequence.add(1);
    sequence.add(7);
    sequence.add(4);
    ASSERT_EQ(sequence.getLength(), 4U);
    EXPECT_EQ(sequence.get(0).value(), 1);
    EXPECT_EQ(sequence.get(1).value(), 4);
    EXPECT_EQ(sequence.get(2).value(), 4);
    EXPECT_EQ(sequence.get(3).value(), 7);
    EXPECT_EQ(sequence.at(3), 7);
    EXPECT_EQ(sequence.indexOf(4), 1U);
}

TEST(ArraySortedSequenceTest, EmptyAndBounds) {
    ArraySortedSequence<int> sequence;
    EXPECT_TRUE(sequence.getIsEmpty());
    EXPECT_FALSE(sequence.getFirst().has_value());
    EXPECT_FALSE(sequence.getLast().has_value());
    EXPECT_FALSE(sequence.get(1).has_value());
    EXPECT_FALSE(sequence.getSubsequence(0, 0).has_value());
}

TEST(ArraySortedSequenceTest, SubsequenceOwnsValues) {
    ArraySortedSequence<int> sequence;
    for (const int value : {5, 1, 3, 2, 4}) sequence.add(value);
    auto subsequence = sequence.getSubsequence(1, 3);
    ASSERT_TRUE(subsequence.has_value());
    EXPECT_EQ(subsequence.value()->getLength(), 3U);
    EXPECT_EQ(subsequence.value()->getFirst().value(), 2);
    EXPECT_EQ(subsequence.value()->getLast().value(), 4);
    EXPECT_FALSE(sequence.getSubsequence(4, 2).has_value());
}

TEST(ArraySortedSequenceTest, CopyAndMove) {
    ArraySortedSequence<int> original;
    original.add(2);
    original.add(1);
    ArraySortedSequence<int> copy = original;
    copy.add(3);
    EXPECT_EQ(original.getLength(), 2U);
    ArraySortedSequence<int> moved = std::move(copy);
    EXPECT_EQ(moved.getLength(), 3U);
    EXPECT_EQ(copy.getLength(), 0U);
}
