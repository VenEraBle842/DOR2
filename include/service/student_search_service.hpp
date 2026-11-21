#pragma once

#include "index/student_index_catalog.hpp"

#include <cmath>
#include <type_traits>

class StudentSearchService {
private:
    StudentRepository repository_;
    StudentIndexCatalog indexes_;

    template <typename Key, typename Compare>
    std::expected<StudentRecordReferences, Error> findExactLinear(
        const StudentAttribute<Key, Compare>& attribute,
        const Key& key
    ) const {
        if constexpr (std::is_floating_point_v<Key>) {
            if (!std::isfinite(key)) return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        }
        const Compare compare{};
        StudentRecordReferences result;
        for (std::size_t position = 0; position < repository_.getCount(); ++position) {
            const Student& student = repository_.at(position);
            const Key studentKey = attribute.extractKey(student);
            if (!compare(studentKey, key) && !compare(key, studentKey)) {
                result.push_back({repository_.getSnapshotId(), position, student.id});
            }
        }
        heapSort(result, [](const StudentRecordReference& left, const StudentRecordReference& right) {
            return left.recordId < right.recordId;
        });
        return result;
    }

    template <typename Key, typename Compare>
    std::expected<StudentRecordReferences, Error> findRangeLinear(
        const StudentAttribute<Key, Compare>& attribute,
        const Key& lower,
        const Key& upper
    ) const {
        if constexpr (std::is_floating_point_v<Key>) {
            if (!std::isfinite(lower) || !std::isfinite(upper)) {
                return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
            }
        }
        const Compare compare{};
        if (compare(upper, lower)) return std::unexpected(makeError(ErrorCode::InvalidRange));

        struct Match {
            Key key{};
            StudentRecordReference reference;
        };

        struct MatchCompare {
            Compare compare{};

            bool operator()(const Match& left, const Match& right) const {
                if (compare(left.key, right.key)) return true;
                if (compare(right.key, left.key)) return false;
                return left.reference.recordId < right.reference.recordId;
            }
        };

        DynamicArray<Match> matches;
        for (std::size_t position = 0; position < repository_.getCount(); ++position) {
            const Student& student = repository_.at(position);
            const Key key = attribute.extractKey(student);
            if (!compare(key, lower) && !compare(upper, key)) {
                matches.push_back({key, {repository_.getSnapshotId(), position, student.id}});
            }
        }
        heapSort(matches, MatchCompare{});

        StudentRecordReferences result;
        for (const auto& match : matches) {
            result.push_back(match.reference);
        }
        return result;
    }

public:
    const StudentRepository& getRepository() const noexcept { return repository_; }
    const StudentIndexCatalog& getIndexes() const noexcept { return indexes_; }

    std::expected<void, Error> addStudent(const Student& student) {
        return repository_.add(student);
    }

    std::expected<void, Error> replaceStudents(DynamicArray<Student> students) {
        return repository_.replaceAll(std::move(students));
    }

    template <typename Key, typename Compare>
    void registerIndex(const StudentAttribute<Key, Compare>& attribute, IndexBackend backend) {
        indexes_.registerIndex(attribute, backend);
    }

    void clearStudents() noexcept { repository_.clear(); }
    void buildIndexes() { indexes_.build(repository_); }

    std::expected<void, Error> buildIndexes(const DynamicArray<IndexSelection>& selections) {
        return indexes_.build(repository_, selections);
    }

    StudentIndexStatistics getIndexStatistics() const {
        return indexes_.getStatistics(repository_);
    }

    std::expected<Student, Error> resolve(
        const StudentRecordReference& reference
    ) const {
        if (reference.snapshotId != repository_.getSnapshotId()) {
            return std::unexpected(makeError(ErrorCode::IndexStale));
        }
        if (reference.position >= repository_.getCount() ||
            repository_.at(reference.position).id != reference.recordId) {
            return std::unexpected(makeError(ErrorCode::Unexpected,
                {.detail = "Invalid student record reference"}));
        }
        return repository_.at(reference.position);
    }

    static std::expected<DynamicArray<RecordId>, Error> recordIds(
        const std::expected<StudentRecordReferences, Error>& records
    ) {
        if (!records) return std::unexpected(records.error());
        DynamicArray<RecordId> ids;
        for (const auto& record : *records) {
            ids.push_back(record.recordId);
        }
        return ids;
    }

    template <typename Attribute>
    std::expected<StudentRecordReferences, Error> findExact(
        const Attribute& attribute,
        const typename Attribute::KeyType& key,
        ExactSearchStrategy strategy
    ) const {
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear(attribute, key);
        }
        if (strategy != ExactSearchStrategy::Tree && strategy != ExactSearchStrategy::Binary) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Search strategy"}));
        }
        const IndexBackend backend = strategy == ExactSearchStrategy::Tree
            ? IndexBackend::Tree : IndexBackend::SortedArray;
        using Key = typename Attribute::KeyType;
        const auto index = indexes_.getIndex<Key>({attribute.id, backend});
        if (!index) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Index"}));
        }
        return index->findExact(repository_, key);
    }

    template <typename Attribute>
    std::expected<StudentRecordReferences, Error> findRange(
        const Attribute& attribute,
        const typename Attribute::KeyType& lower,
        const typename Attribute::KeyType& upper,
        RangeSearchStrategy strategy
    ) const {
        if (strategy == RangeSearchStrategy::Linear) {
            return findRangeLinear(attribute, lower, upper);
        }
        if (strategy != RangeSearchStrategy::Binary) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range strategy"}));
        }
        using Key = typename Attribute::KeyType;
        const auto index = indexes_.getIndex<Key>({attribute.id, IndexBackend::SortedArray});
        if (!index) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range index"}));
        }
        if (!index->supportsRangeSearch()) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range index"}));
        }
        const auto& range = index->rangeSearch();
        return range.findRange(repository_, lower, upper);
    }

    std::expected<DynamicArray<RecordId>, Error> findById(
        RecordId key,
        ExactSearchStrategy strategy
    ) const {
        return recordIds(findExact(studentIdAttribute, key, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findByLastName(
        const std::string& key,
        ExactSearchStrategy strategy
    ) const {
        return recordIds(findExact(studentLastNameAttribute, key, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findByBirthYear(
        int key,
        ExactSearchStrategy strategy
    ) const {
        return recordIds(findExact(studentBirthYearAttribute, key, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findByGpa(
        double key,
        ExactSearchStrategy strategy
    ) const {
        return recordIds(findExact(studentGpaAttribute, key, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findByCompositeKey(
        const StudentCompositeKey& key,
        ExactSearchStrategy strategy
    ) const {
        return recordIds(findExact(studentCompositeAttribute, key, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findInBirthYearRange(
        int lower,
        int upper,
        RangeSearchStrategy strategy
    ) const {
        return recordIds(findRange(studentBirthYearAttribute, lower, upper, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findInGpaRange(
        double lower,
        double upper,
        RangeSearchStrategy strategy
    ) const {
        return recordIds(findRange(studentGpaAttribute, lower, upper, strategy));
    }

    std::expected<DynamicArray<RecordId>, Error> findInCompositeKeyRange(
        const StudentCompositeKey& lower,
        const StudentCompositeKey& upper,
        RangeSearchStrategy strategy
    ) const {
        return recordIds(findRange(studentCompositeAttribute, lower, upper, strategy));
    }
};
