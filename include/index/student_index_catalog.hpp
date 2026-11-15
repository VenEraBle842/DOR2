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
    std::size_t records{0};
    std::size_t lastNameKeys{0};
    std::size_t birthYearKeys{0};
    std::size_t gpaKeys{0};
    std::size_t compositeKeys{0};
};

class StudentIndexCatalog {
private:
    using IdGroup = DynamicArray<RecordId>;
    using IdSequence = ArraySortedSequence<IndexEntry<RecordId>, IndexEntryCompare<RecordId>>;
    using LastNameSequence = ArraySortedSequence<IndexEntry<std::string>, IndexEntryCompare<std::string>>;
    using BirthYearSequence = ArraySortedSequence<IndexEntry<int>, IndexEntryCompare<int>>;
    using GpaSequence = ArraySortedSequence<IndexEntry<double>, IndexEntryCompare<double>>;
    using CompositeSequence = ArraySortedSequence<IndexEntry<StudentCompositeKey>,
        IndexEntryCompare<StudentCompositeKey, StudentCompositeKeyCompare>>;

    TreeDictionary<RecordId, std::size_t> idIndex_;
    TreeDictionary<std::string, IdGroup> lastNameIndex_;
    TreeDictionary<int, IdGroup> birthYearIndex_;
    TreeDictionary<double, IdGroup> gpaIndex_;
    TreeDictionary<StudentCompositeKey, IdGroup, StudentCompositeKeyCompare> compositeIndex_;

    IdSequence idSequence_;
    LastNameSequence lastNameSequence_;
    BirthYearSequence birthYearSequence_;
    GpaSequence gpaSequence_;
    CompositeSequence compositeSequence_;

    std::optional<std::uint64_t> builtSnapshotId_;

    static void insertRecordId(IdGroup& group, RecordId id) {
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
    static void addToGroup(TreeDictionary<Key, IdGroup, Compare>& dictionary, const Key& key, RecordId id) {
        const bool updated = dictionary.modify(key, [id](IdGroup& group) {
            insertRecordId(group, id);
        });
        if (!updated) {
            IdGroup ids;
            ids.push_back(id);
            const auto inserted = dictionary.add(key, ids);
            if (!inserted) {
                throw std::logic_error("duplicate key while creating an index group");
            }
        }
    }

    std::expected<void, IndexError> verifyState(const StudentRepository& repository) const {
        if (!builtSnapshotId_) return std::unexpected(IndexError::NotBuilt);
        if (*builtSnapshotId_ != repository.getSnapshotId()) return std::unexpected(IndexError::Stale);
        return {};
    }

    template <typename Key, typename Compare>
    std::expected<IdGroup, IndexError> findInTree(
        const StudentRepository& repository,
        const TreeDictionary<Key, IdGroup, Compare>& dictionary,
        const Key& key
    ) const {
        if (const auto state = verifyState(repository); !state) return std::unexpected(state.error());
        const auto group = dictionary.find(key);
        return group.value_or(IdGroup{});
    }

    template <typename Key, typename Sequence, typename Compare = std::less<Key>>
    std::expected<IdGroup, IndexError> findInSequence(
        const StudentRepository& repository,
        const Sequence& sequence,
        const Key& key,
        Compare compare = {}
    ) const {
        if (const auto state = verifyState(repository); !state) return std::unexpected(state.error());
        IdGroup result;
        const auto range = equalRange(sequence, key, IndexKeyExtractor<Key>{}, compare);
        for (std::size_t position = range.first; position < range.second; ++position) {
            result.push_back(sequence.at(position).recordId);
        }
        return result;
    }

    template <typename Key, typename Sequence, typename Compare = std::less<Key>>
    std::expected<IdGroup, SearchError> findRangeInSequence(
        const StudentRepository& repository,
        const Sequence& sequence,
        const Key& lower,
        const Key& upper,
        Compare compare = {}
    ) const {
        if (const auto state = verifyState(repository); !state) {
            return std::unexpected(SearchError::VerificationFailed);
        }
        if (compare(upper, lower)) return std::unexpected(SearchError::InvalidRange);
        IdGroup result;
        const std::size_t begin = lowerBound(sequence, lower, IndexKeyExtractor<Key>{}, compare);
        const std::size_t end = upperBound(sequence, upper, IndexKeyExtractor<Key>{}, compare);
        for (std::size_t position = begin; position < end; ++position) {
            result.push_back(sequence.at(position).recordId);
        }
        return result;
    }

public:
    void build(const StudentRepository& repository);

    bool getIsBuilt() const noexcept { return builtSnapshotId_.has_value(); }
    bool getIsStale(const StudentRepository& repository) const noexcept {
        return builtSnapshotId_.has_value() && *builtSnapshotId_ != repository.getSnapshotId();
    }

    std::expected<StudentIndexStatistics, IndexError> getStatistics(
        const StudentRepository& repository
    ) const;

    std::expected<std::optional<std::size_t>, IndexError> findId(
        const StudentRepository& repository,
        RecordId id
    ) const;

    std::expected<IdGroup, IndexError> findIdBinary(
        const StudentRepository& repository, RecordId id
    ) const { return findInSequence(repository, idSequence_, id); }

    std::expected<IdGroup, IndexError> findLastNameTree(
        const StudentRepository& repository, const std::string& key
    ) const { return findInTree(repository, lastNameIndex_, key); }

    std::expected<IdGroup, IndexError> findLastNameBinary(
        const StudentRepository& repository, const std::string& key
    ) const { return findInSequence(repository, lastNameSequence_, key); }

    std::expected<IdGroup, IndexError> findBirthYearTree(
        const StudentRepository& repository, int key
    ) const { return findInTree(repository, birthYearIndex_, key); }

    std::expected<IdGroup, IndexError> findBirthYearBinary(
        const StudentRepository& repository, int key
    ) const { return findInSequence(repository, birthYearSequence_, key); }

    std::expected<IdGroup, IndexError> findGpaTree(
        const StudentRepository& repository, double key
    ) const {
        if (!std::isfinite(key)) return std::unexpected(IndexError::InvalidKey);
        return findInTree(repository, gpaIndex_, key);
    }

    std::expected<IdGroup, IndexError> findGpaBinary(
        const StudentRepository& repository, double key
    ) const {
        if (!std::isfinite(key)) return std::unexpected(IndexError::InvalidKey);
        return findInSequence(repository, gpaSequence_, key);
    }

    std::expected<IdGroup, IndexError> findCompositeTree(
        const StudentRepository& repository, const StudentCompositeKey& key
    ) const { return findInTree(repository, compositeIndex_, key); }

    std::expected<IdGroup, IndexError> findCompositeBinary(
        const StudentRepository& repository, const StudentCompositeKey& key
    ) const {
        return findInSequence(repository, compositeSequence_, key, StudentCompositeKeyCompare{});
    }

    std::expected<IdGroup, SearchError> findBirthYearRange(
        const StudentRepository& repository, int lower, int upper
    ) const { return findRangeInSequence(repository, birthYearSequence_, lower, upper); }

    std::expected<IdGroup, SearchError> findGpaRange(
        const StudentRepository& repository, double lower, double upper
    ) const {
        if (!std::isfinite(lower) || !std::isfinite(upper)) {
            return std::unexpected(SearchError::InvalidKey);
        }
        return findRangeInSequence(repository, gpaSequence_, lower, upper);
    }

    std::expected<IdGroup, SearchError> findCompositeRange(
        const StudentRepository& repository,
        const StudentCompositeKey& lower,
        const StudentCompositeKey& upper
    ) const {
        return findRangeInSequence(
            repository, compositeSequence_, lower, upper, StudentCompositeKeyCompare{}
        );
    }
};
