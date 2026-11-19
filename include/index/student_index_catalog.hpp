#pragma once

#include "common/errors.hpp"
#include "dictionary/tree_dictionary.hpp"
#include "index/index_entry.hpp"
#include "models/student_composite_key.hpp"
#include "search/binary_search.hpp"
#include "sequence/array_sorted_sequence.hpp"
#include "storage/student_repository.hpp"

#include <cstddef>
#include <cmath>
#include <expected>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

struct StudentIndexStatistics {
    std::size_t recordCount{0};
    std::size_t lastNameKeyCount{0};
    std::size_t birthYearKeyCount{0};
    std::size_t gpaKeyCount{0};
    std::size_t compositeKeyCount{0};
};

class StudentIndexCatalog {
private:
    using RecordIds = DynamicArray<RecordId>;
    using IdSortedArrayIndex = ArraySortedSequence<IndexEntry<RecordId>, IndexEntryCompare<RecordId>>;
    using LastNameSortedArrayIndex = ArraySortedSequence<IndexEntry<std::string>,
        IndexEntryCompare<std::string>>;
    using BirthYearSortedArrayIndex = ArraySortedSequence<IndexEntry<int>, IndexEntryCompare<int>>;
    using GpaSortedArrayIndex = ArraySortedSequence<IndexEntry<double>, IndexEntryCompare<double>>;
    using CompositeSortedArrayIndex = ArraySortedSequence<IndexEntry<StudentCompositeKey>,
        IndexEntryCompare<StudentCompositeKey, StudentCompositeKeyCompare>>;

    TreeDictionary<RecordId, std::size_t> idTreeIndex_;
    TreeDictionary<std::string, RecordIds> lastNameTreeIndex_;
    TreeDictionary<int, RecordIds> birthYearTreeIndex_;
    TreeDictionary<double, RecordIds> gpaTreeIndex_;
    TreeDictionary<StudentCompositeKey, RecordIds, StudentCompositeKeyCompare> compositeTreeIndex_;

    IdSortedArrayIndex idSortedArrayIndex_;
    LastNameSortedArrayIndex lastNameSortedArrayIndex_;
    BirthYearSortedArrayIndex birthYearSortedArrayIndex_;
    GpaSortedArrayIndex gpaSortedArrayIndex_;
    CompositeSortedArrayIndex compositeSortedArrayIndex_;

    std::optional<std::uint64_t> indexedSnapshotId_;

    static void insertRecordId(RecordIds& group, RecordId id) {
        std::size_t position = 0;
        while (position < group.size() && group[position] < id) ++position;
        if (position < group.size() && group[position] == id) return;
        group.push_back(id);
        for (std::size_t index = group.size() - 1; index > position; --index) {
            group[index] = group[index - 1];
        }
        group[position] = id;
    }

    template <typename Key, typename Compare>
    static void addToGroup(TreeDictionary<Key, RecordIds, Compare>& dictionary, const Key& key, RecordId id) {
        const bool updated = dictionary.modify(key, [id](RecordIds& group) {
            insertRecordId(group, id);
        });
        if (!updated) {
            RecordIds ids;
            ids.push_back(id);
            const auto inserted = dictionary.add(key, ids);
            if (!inserted) {
                throw std::logic_error("duplicate key while creating an index group");
            }
        }
    }

    std::expected<void, Error> checkIndexState(const StudentRepository& repository) const {
        if (!indexedSnapshotId_) return std::unexpected(makeError(ErrorCode::IndexNotBuilt));
        if (*indexedSnapshotId_ != repository.getSnapshotId()) return std::unexpected(makeError(ErrorCode::IndexStale));
        return {};
    }

    template <typename Key, typename Compare>
    std::expected<RecordIds, Error> findExactTree(
        const StudentRepository& repository,
        const TreeDictionary<Key, RecordIds, Compare>& dictionary,
        const Key& key
    ) const {
        if (const auto state = checkIndexState(repository); !state) return std::unexpected(state.error());
        const auto group = dictionary.find(key);
        return group.value_or(RecordIds{});
    }

    template <typename Key, typename Sequence, typename Compare = std::less<Key>>
    std::expected<RecordIds, Error> findExactBinary(
        const StudentRepository& repository,
        const Sequence& sequence,
        const Key& key,
        Compare compare = {}
    ) const {
        if (const auto state = checkIndexState(repository); !state) return std::unexpected(state.error());
        RecordIds result;
        const auto range = equalRange(sequence, key, IndexEntryKeyExtractor<Key>{}, compare);
        for (std::size_t position = range.first; position < range.second; ++position) {
            result.push_back(sequence.at(position).recordId);
        }
        return result;
    }

    template <typename Key, typename Sequence, typename Compare = std::less<Key>>
    std::expected<RecordIds, Error> findRangeBinary(
        const StudentRepository& repository,
        const Sequence& sequence,
        const Key& lower,
        const Key& upper,
        Compare compare = {}
    ) const {
        if (const auto state = checkIndexState(repository); !state) {
            return std::unexpected(state.error());
        }
        if (compare(upper, lower)) return std::unexpected(makeError(ErrorCode::InvalidRange));
        RecordIds result;
        const std::size_t begin = lowerBound(sequence, lower, IndexEntryKeyExtractor<Key>{}, compare);
        const std::size_t end = upperBound(sequence, upper, IndexEntryKeyExtractor<Key>{}, compare);
        for (std::size_t position = begin; position < end; ++position) {
            result.push_back(sequence.at(position).recordId);
        }
        return result;
    }

public:
    void build(const StudentRepository& repository);

    bool isBuilt() const noexcept { return indexedSnapshotId_.has_value(); }
    bool isStale(const StudentRepository& repository) const noexcept {
        return indexedSnapshotId_.has_value() && *indexedSnapshotId_ != repository.getSnapshotId();
    }

    std::expected<StudentIndexStatistics, Error> getStatistics(
        const StudentRepository& repository
    ) const;

    std::expected<std::optional<std::size_t>, Error> findPositionByIdTree(
        const StudentRepository& repository,
        RecordId id
    ) const;

    std::expected<RecordIds, Error> findByIdBinary(
        const StudentRepository& repository, RecordId id
    ) const { return findExactBinary(repository, idSortedArrayIndex_, id); }

    std::expected<RecordIds, Error> findByLastNameTree(
        const StudentRepository& repository, const std::string& key
    ) const { return findExactTree(repository, lastNameTreeIndex_, key); }

    std::expected<RecordIds, Error> findByLastNameBinary(
        const StudentRepository& repository, const std::string& key
    ) const { return findExactBinary(repository, lastNameSortedArrayIndex_, key); }

    std::expected<RecordIds, Error> findByBirthYearTree(
        const StudentRepository& repository, int key
    ) const { return findExactTree(repository, birthYearTreeIndex_, key); }

    std::expected<RecordIds, Error> findByBirthYearBinary(
        const StudentRepository& repository, int key
    ) const { return findExactBinary(repository, birthYearSortedArrayIndex_, key); }

    std::expected<RecordIds, Error> findByGpaTree(
        const StudentRepository& repository, double key
    ) const {
        if (!std::isfinite(key)) return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        return findExactTree(repository, gpaTreeIndex_, key);
    }

    std::expected<RecordIds, Error> findByGpaBinary(
        const StudentRepository& repository, double key
    ) const {
        if (!std::isfinite(key)) return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        return findExactBinary(repository, gpaSortedArrayIndex_, key);
    }

    std::expected<RecordIds, Error> findByCompositeKeyTree(
        const StudentRepository& repository, const StudentCompositeKey& key
    ) const { return findExactTree(repository, compositeTreeIndex_, key); }

    std::expected<RecordIds, Error> findByCompositeKeyBinary(
        const StudentRepository& repository, const StudentCompositeKey& key
    ) const {
        return findExactBinary(repository, compositeSortedArrayIndex_, key, StudentCompositeKeyCompare{});
    }

    std::expected<RecordIds, Error> findInBirthYearRangeBinary(
        const StudentRepository& repository, int lower, int upper
    ) const { return findRangeBinary(repository, birthYearSortedArrayIndex_, lower, upper); }

    std::expected<RecordIds, Error> findInGpaRangeBinary(
        const StudentRepository& repository, double lower, double upper
    ) const {
        if (!std::isfinite(lower) || !std::isfinite(upper)) {
            return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        }
        return findRangeBinary(repository, gpaSortedArrayIndex_, lower, upper);
    }

    std::expected<RecordIds, Error> findInCompositeKeyRangeBinary(
        const StudentRepository& repository,
        const StudentCompositeKey& lower,
        const StudentCompositeKey& upper
    ) const {
        return findRangeBinary(
            repository, compositeSortedArrayIndex_, lower, upper, StudentCompositeKeyCompare{}
        );
    }
};
