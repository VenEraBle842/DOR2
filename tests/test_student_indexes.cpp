#include <gtest/gtest.h>

#include "index/student_index_catalog.hpp"

#include <limits>
#include <utility>

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

    ASSERT_EQ(catalog.findPositionByIdTree(repository, 20)->value(), 2U);
    ASSERT_EQ(catalog.findByLastNameTree(repository, "Smith")->size(), 3U);
    EXPECT_EQ((*catalog.findByLastNameTree(repository, "Smith"))[0], 10U);
    EXPECT_EQ(catalog.findByBirthYearBinary(repository, 2003)->size(), 3U);
    EXPECT_EQ(catalog.findByGpaTree(repository, 4.5)->size(), 2U);
    EXPECT_EQ(catalog.findByCompositeKeyBinary(
        repository, StudentCompositeKey{"Smith", "Ivan", 2003}
    )->size(), 2U);
}

TEST(StudentIndexCatalogTest, TreeAndBinaryResultsAreEqualAndUnique) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto tree = catalog.findByLastNameTree(repository, "Smith").value();
    const auto binary = catalog.findByLastNameBinary(repository, "Smith").value();
    ASSERT_EQ(tree.size(), binary.size());
    for (std::size_t index = 0; index < tree.size(); ++index) EXPECT_EQ(tree[index], binary[index]);
}

TEST(StudentIndexCatalogTest, RangeIsInclusiveAndOrderedByKeyThenId) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto years = catalog.findInBirthYearRangeBinary(repository, 2002, 2003).value();
    ASSERT_EQ(years.size(), 4U);
    EXPECT_EQ(years[0], 10U);
    EXPECT_EQ(years[1], 20U);
    EXPECT_EQ(years[2], 30U);
    EXPECT_EQ(years[3], 40U);
    EXPECT_EQ(catalog.findInGpaRangeBinary(repository, 5.0, 4.0).error().code, ErrorCode::InvalidRange);
}

TEST(StudentIndexCatalogTest, RangeQueriesPreserveStateErrorsBeforeCheckingRangeOrder) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    for (const auto error : {ErrorCode::IndexNotBuilt, ErrorCode::IndexStale}) {
        if (error == ErrorCode::IndexStale) {
            catalog.build(repository);
            ASSERT_TRUE(repository.add(Student{50, "New", "Student", 2000, 4.0}).has_value());
        }
        EXPECT_EQ(catalog.findInBirthYearRangeBinary(repository, 2002, 2004).error().code, error);
        EXPECT_EQ(catalog.findInGpaRangeBinary(repository, 4.0, 5.0).error().code, error);
        EXPECT_EQ(catalog.findInCompositeKeyRangeBinary(
            repository, {"Brown", "Ivan", 2003}, {"Smith", "Ivan", 2003}
        ).error().code, error);
        EXPECT_EQ(catalog.findInBirthYearRangeBinary(repository, 2004, 2002).error().code, error);
        EXPECT_EQ(catalog.findInGpaRangeBinary(repository, 5.0, 4.0).error().code, error);
        EXPECT_EQ(catalog.findInCompositeKeyRangeBinary(
            repository, {"Smith", "Ivan", 2003}, {"Brown", "Ivan", 2003}
        ).error().code, error);
    }
}

TEST(StudentIndexCatalogTest, RangeQueriesDistinguishInvalidBoundsFromStateErrors) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    EXPECT_EQ(catalog.findInBirthYearRangeBinary(repository, 2004, 2002).error().code, ErrorCode::InvalidRange);
    EXPECT_EQ(catalog.findInGpaRangeBinary(repository, 5.0, 4.0).error().code, ErrorCode::InvalidRange);
    EXPECT_EQ(catalog.findInCompositeKeyRangeBinary(
        repository, {"Smith", "Ivan", 2003}, {"Brown", "Ivan", 2003}
    ).error().code, ErrorCode::InvalidRange);
    for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()}) {
        EXPECT_EQ(catalog.findInGpaRangeBinary(repository, invalid, 5.0).error().code, ErrorCode::NonFiniteNumber);
        EXPECT_EQ(catalog.findInGpaRangeBinary(repository, 4.0, invalid).error().code, ErrorCode::NonFiniteNumber);
    }
}

TEST(StudentIndexCatalogTest, DetectsNotBuiltAndStaleThenRebuilds) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    EXPECT_EQ(catalog.findByLastNameTree(repository, "Smith").error().code, ErrorCode::IndexNotBuilt);
    catalog.build(repository);
    ASSERT_TRUE(repository.add(Student{50, "Nina", "Smith", 2001, 4.2}).has_value());
    EXPECT_EQ(catalog.findByLastNameTree(repository, "Smith").error().code, ErrorCode::IndexStale);
    catalog.build(repository);
    EXPECT_EQ(catalog.findByLastNameTree(repository, "Smith")->size(), 4U);
}

TEST(StudentRepositoryTest, RejectsDuplicateAndReplacementIsTransactional) {
    auto repository = makeRepository();
    EXPECT_EQ(repository.add(Student{30, "Other", "Name", 2000, 1.0}).error().code,
              ErrorCode::DuplicateId);
    DynamicArray<Student> duplicate;
    duplicate.push_back(Student{1, "A", "A", 2000, 4.0});
    duplicate.push_back(Student{1, "B", "B", 2001, 4.1});
    EXPECT_FALSE(repository.replaceAll(std::move(duplicate)).has_value());
    EXPECT_EQ(repository.getCount(), 4U);
}

TEST(StudentIndexCatalogTest, RejectsAnotherRepositoryWithSameSizeAndNumberOfChanges) {
    auto original = makeRepository();
    auto unrelated = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(original);
    EXPECT_NE(original.getSnapshotId(), unrelated.getSnapshotId());
    EXPECT_EQ(catalog.findPositionByIdTree(unrelated, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findByLastNameTree(unrelated, "Smith").error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findByLastNameBinary(unrelated, "Smith").error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.getStatistics(unrelated).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findInBirthYearRangeBinary(unrelated, 2002, 2004).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findInGpaRangeBinary(unrelated, 4.0, 5.0).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findInCompositeKeyRangeBinary(
        unrelated, {"Brown", "Ivan", 2003}, {"Smith", "Ivan", 2003}
    ).error().code, ErrorCode::IndexStale);
}

TEST(StudentRepositoryTest, CopiesShareSnapshotUntilIndependentlyChanged) {
    auto original = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(original);
    auto copy = original;
    StudentRepository assigned;
    ASSERT_TRUE(assigned.add(Student{99, "Old", "Data", 1999, 3.0}).has_value());
    assigned = original;
    EXPECT_EQ(copy.getSnapshotId(), original.getSnapshotId());
    EXPECT_EQ(assigned.getSnapshotId(), original.getSnapshotId());
    EXPECT_EQ(catalog.findPositionByIdTree(copy, 20)->value(), 2U);
    EXPECT_EQ(catalog.findPositionByIdTree(assigned, 20)->value(), 2U);
    EXPECT_FALSE(assigned.findById(99).has_value());
    ASSERT_TRUE(copy.add(Student{50, "New", "Student", 2000, 4.0}).has_value());
    EXPECT_EQ(catalog.findPositionByIdTree(copy, 20).error().code, ErrorCode::IndexStale);
    EXPECT_FALSE(original.findById(50).has_value());
    assigned.clear();
    EXPECT_EQ(catalog.findPositionByIdTree(assigned, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(catalog.findPositionByIdTree(original, 20)->value(), 2U);
    EXPECT_EQ(original.getCount(), 4U);
}

TEST(StudentRepositoryTest, MovesTransferSnapshotAndResetSource) {
    auto source = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(source);
    const auto snapshot = source.getSnapshotId();
    auto moved = std::move(source);
    EXPECT_EQ(moved.getSnapshotId(), snapshot);
    EXPECT_EQ(catalog.findPositionByIdTree(moved, 20)->value(), 2U);
    EXPECT_EQ(source.getCount(), 0U);
    EXPECT_NE(source.getSnapshotId(), snapshot);
    EXPECT_EQ(catalog.findPositionByIdTree(source, 20).error().code, ErrorCode::IndexStale);
    ASSERT_TRUE(source.add(Student{20, "Reuse", "Source", 2000, 4.0}).has_value());
    EXPECT_EQ(moved.findById(20)->firstName, "Ivan");

    auto assigned = makeRepository();
    StudentIndexCatalog oldCatalog;
    oldCatalog.build(assigned);
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getSnapshotId(), snapshot);
    EXPECT_EQ(catalog.findPositionByIdTree(assigned, 20)->value(), 2U);
    EXPECT_EQ(oldCatalog.findPositionByIdTree(assigned, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(moved.getCount(), 0U);
    EXPECT_NE(moved.getSnapshotId(), snapshot);
    EXPECT_EQ(catalog.findPositionByIdTree(moved, 20).error().code, ErrorCode::IndexStale);
    ASSERT_TRUE(moved.add(Student{20, "Reuse", "Moved", 2000, 4.0}).has_value());
    EXPECT_EQ(assigned.findById(20)->firstName, "Ivan");
}

TEST(StudentRepositoryTest, RejectsNonFiniteGpaWithoutChangingDataOrSnapshot) {
    auto repository = makeRepository();
    const auto snapshot = repository.getSnapshotId();
    const Student first = repository.at(0);
    for (const double gpa : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()}) {
        EXPECT_EQ(repository.add(Student{50, "Bad", "Gpa", 2000, gpa}).error().code,
                  ErrorCode::NonFiniteNumber);
        DynamicArray<Student> replacement{
            Student{60, "Valid", "First", 2000, 4.0}, Student{50, "Bad", "Gpa", 2000, gpa}
        };
        EXPECT_EQ(repository.replaceAll(std::move(replacement)).error().code, ErrorCode::NonFiniteNumber);
        EXPECT_EQ(repository.getCount(), 4U);
        EXPECT_EQ(repository.at(0), first);
        EXPECT_EQ(repository.findById(first.id).value(), first);
        EXPECT_FALSE(repository.findById(50).has_value());
        EXPECT_FALSE(repository.findById(60).has_value());
        EXPECT_EQ(repository.getSnapshotId(), snapshot);
    }
}
