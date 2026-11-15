#include <gtest/gtest.h>

#include "index/student_index_catalog.hpp"

StudentRepository makeRepository() {
    StudentRepository repository;
    EXPECT_TRUE(repository.add(Student{30, "Ivan", "Smith", 2003, 4.5}).has_value());
    EXPECT_TRUE(repository.add(Student{10, "Anna", "Smith", 2002, 4.8}).has_value());
    EXPECT_TRUE(repository.add(Student{20, "Ivan", "Brown", 2003, 4.5}).has_value());
    EXPECT_TRUE(repository.add(Student{40, "Ivan", "Smith", 2003, 4.0}).has_value());
    return repository;
}

TEST(StudentIndexCatalogTest, SupportsAllExactIndexes) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);

    ASSERT_EQ(catalog.findId(repository, 20)->value(), 2U);
    ASSERT_EQ(catalog.findLastNameTree(repository, "Smith")->size(), 3U);
    EXPECT_EQ((*catalog.findLastNameTree(repository, "Smith"))[0], 10U);
    EXPECT_EQ(catalog.findBirthYearBinary(repository, 2003)->size(), 3U);
    EXPECT_EQ(catalog.findGpaTree(repository, 4.5)->size(), 2U);
    EXPECT_EQ(catalog.findCompositeBinary(
        repository, StudentCompositeKey{"Smith", "Ivan", 2003}
    )->size(), 2U);
}

TEST(StudentIndexCatalogTest, TreeAndBinaryResultsAreEqualAndUnique) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto tree = catalog.findLastNameTree(repository, "Smith").value();
    const auto binary = catalog.findLastNameBinary(repository, "Smith").value();
    ASSERT_EQ(tree.size(), binary.size());
    for (std::size_t index = 0; index < tree.size(); ++index) EXPECT_EQ(tree[index], binary[index]);
}

TEST(StudentIndexCatalogTest, RangeIsInclusiveAndOrderedByKeyThenId) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto years = catalog.findBirthYearRange(repository, 2002, 2003).value();
    ASSERT_EQ(years.size(), 4U);
    EXPECT_EQ(years[0], 10U);
    EXPECT_EQ(years[1], 20U);
    EXPECT_EQ(years[2], 30U);
    EXPECT_EQ(years[3], 40U);
    EXPECT_EQ(catalog.findGpaRange(repository, 5.0, 4.0).error(), SearchError::InvalidRange);
}

TEST(StudentIndexCatalogTest, DetectsNotBuiltAndStaleThenRebuilds) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    EXPECT_EQ(catalog.findLastNameTree(repository, "Smith").error(), IndexError::NotBuilt);
    catalog.build(repository);
    ASSERT_TRUE(repository.add(Student{50, "Nina", "Smith", 2001, 4.2}).has_value());
    EXPECT_EQ(catalog.findLastNameTree(repository, "Smith").error(), IndexError::Stale);
    catalog.build(repository);
    EXPECT_EQ(catalog.findLastNameTree(repository, "Smith")->size(), 4U);
}

TEST(StudentRepositoryTest, RejectsDuplicateAndReplacementIsTransactional) {
    auto repository = makeRepository();
    EXPECT_EQ(repository.add(Student{30, "Other", "Name", 2000, 1.0}).error(),
              RepositoryError::DuplicateId);
    DynamicArray<Student> duplicate;
    duplicate.push_back(Student{1, "A", "A", 2000, 4.0});
    duplicate.push_back(Student{1, "B", "B", 2001, 4.1});
    EXPECT_FALSE(repository.replaceAll(std::move(duplicate)).has_value());
    EXPECT_EQ(repository.getCount(), 4U);
}
