#include <gtest/gtest.h>

#include "index/student_index_catalog.hpp"

#include <limits>
#include <utility>

namespace {
template <typename Attribute>
std::expected<DynamicArray<RecordId>, Error> findExact(
    const StudentIndexCatalog& catalog, const Attribute& attribute,
    IndexBackend backend, const StudentRepository& repository, const typename Attribute::KeyType& key
) {
    using Key = typename Attribute::KeyType;
    const auto index = catalog.getIndex<Key>({attribute.id, backend});
    if (!index) return std::unexpected(makeError(ErrorCode::UnknownSelection));
    const auto result = index->findExact(repository, key);
    if (!result) return std::unexpected(result.error());
    DynamicArray<RecordId> ids;
    for (const auto& reference : *result) ids.push_back(reference.recordId);
    return ids;
}
template <typename Attribute>
std::expected<DynamicArray<RecordId>, Error> findRange(
    const StudentIndexCatalog& catalog, const Attribute& attribute,
    const StudentRepository& repository,
    const typename Attribute::KeyType& lower, const typename Attribute::KeyType& upper
) {
    using Key = typename Attribute::KeyType;
    const auto index = catalog.getIndex<Key>({attribute.id, IndexBackend::SortedArray});
    if (!index) return std::unexpected(makeError(ErrorCode::UnknownSelection));
    const auto result = index->rangeSearch().findRange(repository, lower, upper);
    if (!result) return std::unexpected(result.error());
    DynamicArray<RecordId> ids;
    for (const auto& reference : *result) ids.push_back(reference.recordId);
    return ids;
}
std::expected<std::optional<std::size_t>, Error> findPosition(
    const StudentIndexCatalog& catalog, const StudentRepository& repository, RecordId id
) {
    const auto result = catalog.getIndex<RecordId>({SearchAttribute::Id, IndexBackend::Tree})->findExact(repository, id);
    if (!result) return std::unexpected(result.error());
    if (result->empty()) return std::optional<std::size_t>{};
    return (*result)[0].position;
}
} // namespace

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

    ASSERT_EQ(findPosition(catalog, repository, 20)->value(), 2U);
    ASSERT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith")->size(), 3U);
    EXPECT_EQ((*findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith"))[0], 10U);
    EXPECT_EQ(findExact(catalog, studentBirthYearAttribute, IndexBackend::SortedArray, repository, 2003)->size(), 3U);
    EXPECT_EQ(findExact(catalog, studentGpaAttribute, IndexBackend::Tree, repository, 4.5)->size(), 2U);
    EXPECT_EQ(findExact(catalog, studentCompositeAttribute, IndexBackend::SortedArray,
        repository, StudentCompositeKey{"Smith", "Ivan", 2003}
    )->size(), 2U);
}

TEST(StudentIndexCatalogTest, TreeAndBinaryResultsAreEqualAndUnique) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto tree = findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith").value();
    const auto binary = findExact(catalog, studentLastNameAttribute, IndexBackend::SortedArray, repository, "Smith").value();
    ASSERT_EQ(tree.size(), binary.size());
    for (std::size_t index = 0; index < tree.size(); ++index) EXPECT_EQ(tree[index], binary[index]);
}

TEST(StudentIndexCatalogTest, RangeIsInclusiveAndOrderedByKeyThenId) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    const auto years = findRange(catalog, studentBirthYearAttribute, repository, 2002, 2003).value();
    ASSERT_EQ(years.size(), 4U);
    EXPECT_EQ(years[0], 10U);
    EXPECT_EQ(years[1], 20U);
    EXPECT_EQ(years[2], 30U);
    EXPECT_EQ(years[3], 40U);
    EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, 5.0, 4.0).error().code, ErrorCode::InvalidRange);
}

TEST(StudentIndexCatalogTest, RangeQueriesPreserveStateErrorsBeforeCheckingRangeOrder) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    for (const auto error : {ErrorCode::IndexNotBuilt, ErrorCode::IndexStale}) {
        if (error == ErrorCode::IndexStale) {
            catalog.build(repository);
            ASSERT_TRUE(repository.add(Student{50, "New", "Student", 2000, 4.0}).has_value());
        }
        EXPECT_EQ(findRange(catalog, studentBirthYearAttribute, repository, 2002, 2004).error().code, error);
        EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, 4.0, 5.0).error().code, error);
        EXPECT_EQ(findRange(catalog, studentCompositeAttribute,
            repository, {"Brown", "Ivan", 2003}, {"Smith", "Ivan", 2003}
        ).error().code, error);
        EXPECT_EQ(findRange(catalog, studentBirthYearAttribute, repository, 2004, 2002).error().code, error);
        EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, 5.0, 4.0).error().code, error);
        EXPECT_EQ(findRange(catalog, studentCompositeAttribute,
            repository, {"Smith", "Ivan", 2003}, {"Brown", "Ivan", 2003}
        ).error().code, error);
    }
}

TEST(StudentIndexCatalogTest, RangeQueriesDistinguishInvalidBoundsFromStateErrors) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(repository);
    EXPECT_EQ(findRange(catalog, studentBirthYearAttribute, repository, 2004, 2002).error().code, ErrorCode::InvalidRange);
    EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, 5.0, 4.0).error().code, ErrorCode::InvalidRange);
    EXPECT_EQ(findRange(catalog, studentCompositeAttribute,
        repository, {"Smith", "Ivan", 2003}, {"Brown", "Ivan", 2003}
    ).error().code, ErrorCode::InvalidRange);
    for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()}) {
        EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, invalid, 5.0).error().code, ErrorCode::NonFiniteNumber);
        EXPECT_EQ(findRange(catalog, studentGpaAttribute, repository, 4.0, invalid).error().code, ErrorCode::NonFiniteNumber);
    }
}

TEST(StudentIndexCatalogTest, DetectsNotBuiltAndStaleThenRebuilds) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith").error().code, ErrorCode::IndexNotBuilt);
    catalog.build(repository);
    ASSERT_TRUE(repository.add(Student{50, "Nina", "Smith", 2001, 4.2}).has_value());
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith").error().code, ErrorCode::IndexStale);
    catalog.build(repository);
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, repository, "Smith")->size(), 4U);
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
    EXPECT_EQ(findPosition(catalog, unrelated, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::Tree, unrelated, "Smith").error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::SortedArray, unrelated, "Smith").error().code, ErrorCode::IndexStale);
    const auto statistics = catalog.getStatistics(unrelated);
    EXPECT_EQ(statistics.recordCount, unrelated.getCount());
    for (const auto& state : statistics.indexes) {
        EXPECT_TRUE(state.built);
        EXPECT_TRUE(state.stale);
    }
    EXPECT_EQ(findRange(catalog, studentBirthYearAttribute, unrelated, 2002, 2004).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findRange(catalog, studentGpaAttribute, unrelated, 4.0, 5.0).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findRange(catalog, studentCompositeAttribute,
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
    EXPECT_EQ(findPosition(catalog, copy, 20)->value(), 2U);
    EXPECT_EQ(findPosition(catalog, assigned, 20)->value(), 2U);
    EXPECT_FALSE(assigned.findById(99).has_value());
    ASSERT_TRUE(copy.add(Student{50, "New", "Student", 2000, 4.0}).has_value());
    EXPECT_EQ(findPosition(catalog, copy, 20).error().code, ErrorCode::IndexStale);
    EXPECT_FALSE(original.findById(50).has_value());
    assigned.clear();
    EXPECT_EQ(findPosition(catalog, assigned, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findPosition(catalog, original, 20)->value(), 2U);
    EXPECT_EQ(original.getCount(), 4U);
}

TEST(StudentRepositoryTest, MovesTransferSnapshotAndResetSource) {
    auto source = makeRepository();
    StudentIndexCatalog catalog;
    catalog.build(source);
    const auto snapshot = source.getSnapshotId();
    auto moved = std::move(source);
    EXPECT_EQ(moved.getSnapshotId(), snapshot);
    EXPECT_EQ(findPosition(catalog, moved, 20)->value(), 2U);
    EXPECT_EQ(source.getCount(), 0U);
    EXPECT_NE(source.getSnapshotId(), snapshot);
    EXPECT_EQ(findPosition(catalog, source, 20).error().code, ErrorCode::IndexStale);
    ASSERT_TRUE(source.add(Student{20, "Reuse", "Source", 2000, 4.0}).has_value());
    EXPECT_EQ(moved.findById(20)->firstName, "Ivan");

    auto assigned = makeRepository();
    StudentIndexCatalog oldCatalog;
    oldCatalog.build(assigned);
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getSnapshotId(), snapshot);
    EXPECT_EQ(findPosition(catalog, assigned, 20)->value(), 2U);
    EXPECT_EQ(findPosition(oldCatalog, assigned, 20).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(moved.getCount(), 0U);
    EXPECT_NE(moved.getSnapshotId(), snapshot);
    EXPECT_EQ(findPosition(catalog, moved, 20).error().code, ErrorCode::IndexStale);
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

TEST(StudentIndexCatalogTest, SelectiveBuildTracksEveryAttributeAndBackendSeparately) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    const IndexSelection yearTree{SearchAttribute::BirthYear, IndexBackend::Tree};
    const IndexSelection yearArray{SearchAttribute::BirthYear, IndexBackend::SortedArray};
    const IndexSelection nameArray{SearchAttribute::LastName, IndexBackend::SortedArray};
    EXPECT_FALSE(catalog.isBuilt());
    ASSERT_EQ(catalog.getStates(repository).size(), 10U);
    ASSERT_TRUE(catalog.build(repository, {yearTree, nameArray, yearTree}));
    for (const auto& state : catalog.getStates(repository)) {
        EXPECT_EQ(state.built, state.selection == yearTree || state.selection == nameArray);
        EXPECT_FALSE(state.stale);
    }
    EXPECT_EQ(findExact(catalog, studentBirthYearAttribute, IndexBackend::Tree, repository, 2003)->size(), 3U);
    EXPECT_EQ(findRange(catalog, studentBirthYearAttribute, repository, 2002, 2004).error().code, ErrorCode::IndexNotBuilt);
    ASSERT_TRUE(repository.add({50, "A", "Smith", 2005, 4.0}));
    ASSERT_TRUE(catalog.build(repository, {yearArray}));
    EXPECT_EQ(findExact(catalog, studentBirthYearAttribute, IndexBackend::Tree, repository, 2003).error().code, ErrorCode::IndexStale);
    EXPECT_EQ(findExact(catalog, studentLastNameAttribute, IndexBackend::SortedArray, repository, "Smith").error().code, ErrorCode::IndexStale);
    const auto years = findRange(catalog, studentBirthYearAttribute, repository, 2002, 2005);
    ASSERT_TRUE(years);
    ASSERT_EQ(years->size(), 5U);
    EXPECT_EQ((*years)[4], 50U);
    EXPECT_FALSE(catalog.getIndex(yearTree)->supportsRangeSearch());
    EXPECT_THROW(catalog.getIndex<int>(yearTree)->rangeSearch(), std::logic_error);
}

TEST(StudentIndexCatalogTest, InvalidSelectionPreservesAllPreviouslyBuiltStates) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    const IndexSelection selected{SearchAttribute::BirthYear, IndexBackend::SortedArray};
    ASSERT_TRUE(catalog.build(repository, {selected}));
    ASSERT_TRUE(repository.add({50, "A", "Smith", 2005, 4.0}));
    EXPECT_EQ(catalog.build(repository, {selected, {static_cast<SearchAttribute>(99), IndexBackend::Tree}}).error().code,
              ErrorCode::UnknownSelection);
    EXPECT_TRUE(catalog.getIndex(selected)->isStale(repository));
    ASSERT_TRUE(catalog.build(repository, {}));
    EXPECT_TRUE(catalog.getIndex(selected)->isStale(repository));
    EXPECT_FALSE(catalog.getIndex({SearchAttribute::Id, IndexBackend::Tree})->isBuilt());
}

TEST(StudentIndexCatalogTest, CopiesAndMovesPreserveSelectiveBuildWithoutSharingMutableIndexes) {
    auto repository = makeRepository();
    const IndexSelection selected{SearchAttribute::LastName, IndexBackend::SortedArray};
    StudentIndexCatalog original;
    ASSERT_TRUE(original.build(repository, {selected}));
    auto copy = original;
    StudentIndexCatalog assigned;
    assigned = original;
    ASSERT_TRUE(repository.add({50, "A", "Smith", 2005, 4.0}));
    ASSERT_TRUE(copy.build(repository, {selected}));
    EXPECT_TRUE(original.getIndex(selected)->isStale(repository));
    EXPECT_TRUE(assigned.getIndex(selected)->isStale(repository));
    EXPECT_EQ(findExact(copy, studentLastNameAttribute, IndexBackend::SortedArray, repository, "Smith")->size(), 4U);
    StudentIndexCatalog moved(std::move(copy));
    assigned = std::move(moved);
    EXPECT_EQ(findExact(assigned, studentLastNameAttribute, IndexBackend::SortedArray, repository, "Smith")->size(), 4U);
    EXPECT_FALSE(assigned.getIndex({SearchAttribute::Id, IndexBackend::Tree})->isBuilt());
}

TEST(StudentIndexCatalogTest, GenericIndexAcceptsAnotherExtractorWithoutCatalogChanges) {
    const StudentAttribute<int> firstNameLength{static_cast<SearchAttribute>(99), "First name length",
        [](const Student& student) { return static_cast<int>(student.firstName.size()); }};
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    for (const auto backend : {IndexBackend::Tree, IndexBackend::SortedArray}) {
        catalog.registerIndex(firstNameLength, backend);
        ASSERT_TRUE(catalog.build(repository, {{firstNameLength.id, backend}}));
        const auto result = findExact(catalog, firstNameLength, backend, repository, 4);
        ASSERT_TRUE(result);
        ASSERT_EQ(result->size(), 4U);
        EXPECT_EQ((*result)[0], 10U);
        EXPECT_EQ((*result)[3], 40U);
    }
    EXPECT_EQ(findRange(catalog, firstNameLength, repository, 4, 4)->size(), 4U);
    EXPECT_THROW(catalog.registerIndex(firstNameLength, IndexBackend::Tree), std::invalid_argument);
}

namespace {

template <typename Key, typename Compare>
void expectIndexCopyAndMove(
    const StudentRepository& repository,
    const StudentAttribute<Key, Compare>& attribute,
    IndexBackend backend
) {
    using Dictionary = IDictionary<Key, StudentRecordReferences>;
    std::unique_ptr<Dictionary> dictionary;
    if (backend == IndexBackend::Tree) {
        dictionary = std::make_unique<TreeDictionary<Key, StudentRecordReferences, Compare>>();
    } else {
        dictionary = std::make_unique<SortedArrayDictionary<Key, StudentRecordReferences, Compare>>();
    }
    StudentIndex<Key, Compare> original(std::move(dictionary), attribute.extractKey);
    original.build(repository);
    StudentIndex<Key, Compare> copy(original);
    const Key key = attribute.extractKey(repository.at(0));
    const auto expected = original.findExact(repository, key);
    ASSERT_TRUE(expected);

    StudentIndex<Key, Compare> moved(std::move(copy));
    EXPECT_FALSE(copy.isBuilt());
    EXPECT_EQ(copy.findExact(repository, key).error().code, ErrorCode::IndexNotBuilt);
    const auto actual = moved.findExact(repository, key);
    ASSERT_TRUE(actual);
    ASSERT_EQ(actual->size(), expected->size());
    for (std::size_t index = 0; index < actual->size(); ++index) {
        EXPECT_EQ((*actual)[index], (*expected)[index]);
    }

    copy = original;
    moved = std::move(copy);
    EXPECT_FALSE(copy.isBuilt());
    EXPECT_EQ(moved.getKeyCount(), original.getKeyCount());
    EXPECT_EQ(moved.supportsRangeSearch(), backend == IndexBackend::SortedArray);
}

} // namespce

TEST(StudentIndexTest, ConcreteGenericIndexCopiesAndMovesBothBackendsAcrossKeyTypes) {
    const auto repository = makeRepository();
    for (const auto backend : {IndexBackend::Tree, IndexBackend::SortedArray}) {
        expectIndexCopyAndMove(repository, studentBirthYearAttribute, backend);
        expectIndexCopyAndMove(repository, studentLastNameAttribute, backend);
    }
}

TEST(StudentIndexCatalogTest, SelectedIndexRemainsValidAfterRebuildAndCatalogReplacement) {
    auto repository = makeRepository();
    const auto originalSnapshot = repository;
    const IndexSelection selected{SearchAttribute::LastName, IndexBackend::SortedArray};
    const std::string key = "Smith";
    StudentIndexCatalog catalog;
    ASSERT_TRUE(catalog.build(repository, {selected}));
    const auto previousIndex = catalog.getIndex<std::string>(selected);
    ASSERT_TRUE(previousIndex);
    EXPECT_FALSE(catalog.getIndex({static_cast<SearchAttribute>(99), IndexBackend::Tree}));

    ASSERT_TRUE(repository.add({50, "A", "Smith", 2005, 4.0}));
    ASSERT_TRUE(catalog.build(repository, {selected}));
    const auto currentIndex = catalog.getIndex<std::string>(selected);
    ASSERT_TRUE(currentIndex);
    EXPECT_NE(previousIndex, currentIndex);
    EXPECT_TRUE(previousIndex->isStale(repository));
    const auto currentMatches = currentIndex->findExact(repository, key);
    ASSERT_TRUE(currentMatches);
    EXPECT_EQ(currentMatches->size(), 4U);

    catalog = StudentIndexCatalog{};
    const auto previousMatches = previousIndex->findExact(originalSnapshot, key);
    ASSERT_TRUE(previousMatches);
    EXPECT_EQ(previousMatches->size(), 3U);
}

TEST(StudentIndexCatalogTest, TypedLookupRejectsMismatchedKeyTypes) {
    const auto repository = makeRepository();
    StudentIndexCatalog catalog;
    const IndexSelection selected{SearchAttribute::BirthYear, IndexBackend::Tree};
    ASSERT_TRUE(catalog.build(repository, {selected}));
    const auto index = catalog.getIndex<int>(selected);
    ASSERT_TRUE(index);
    const auto matches = index->findExact(repository, 2003);
    ASSERT_TRUE(matches);
    EXPECT_EQ(matches->size(), 3U);
    EXPECT_FALSE(catalog.getIndex<std::string>(selected));
    EXPECT_FALSE(catalog.getIndex<double>(selected));
    EXPECT_FALSE(catalog.getIndex<int>({static_cast<SearchAttribute>(99), IndexBackend::Tree}));
}

TEST(StudentIndexCatalogTest, StatisticsIncludeUnbuiltStaleAndCustomIndexes) {
    auto repository = makeRepository();
    StudentIndexCatalog catalog;
    const auto initial = catalog.getStatistics(repository);
    EXPECT_EQ(initial.recordCount, repository.getCount());
    for (const auto& state : initial.indexes) {
        EXPECT_FALSE(state.built);
        EXPECT_FALSE(state.stale);
        EXPECT_EQ(state.keyCount, 0U);
    }

    const StudentAttribute<int> nameLength{
        static_cast<SearchAttribute>(99), "First name length",
        [](const Student& student) { return static_cast<int>(student.firstName.size()); }
    };
    const IndexSelection selected{nameLength.id, IndexBackend::SortedArray};
    catalog.registerIndex(nameLength, selected.backend);
    ASSERT_TRUE(catalog.build(repository, {selected}));
    const auto built = catalog.getStatistics(repository);
    ASSERT_EQ(built.indexes.size(), initial.indexes.size() + 1);
    for (const auto& state : built.indexes) {
        if (state.selection == selected) {
            EXPECT_TRUE(state.built);
            EXPECT_FALSE(state.stale);
            EXPECT_EQ(state.keyCount, 1U);
            EXPECT_EQ(state.name, nameLength.name);
        } else {
            EXPECT_FALSE(state.built);
        }
    }

    ASSERT_TRUE(repository.add({50, "N", "New", 2005, 4.0}));
    const auto stale = catalog.getStatistics(repository);
    EXPECT_EQ(stale.recordCount, repository.getCount());
    for (const auto& state : stale.indexes) {
        EXPECT_EQ(state.stale, state.selection == selected);
    }
}
