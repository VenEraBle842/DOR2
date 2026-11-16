#pragma once

#include "common/errors.hpp"
#include "common/heap_sort.hpp"
#include "index/student_index_catalog.hpp"
#include "storage/student_repository.hpp"

#include <cstddef>
#include <cmath>
#include <expected>
#include <functional>
#include <string>

enum class ExactSearchStrategy { Linear, Tree, Binary };
enum class RangeSearchStrategy { Linear, Binary };

enum class ServiceError {
    IndexNotBuilt,
    IndexStale,
    InvalidRange,
    InvalidKey,
    UnexpectedIndexError
};

class StudentSearchService {
private:
    StudentRepository repository_;
    StudentIndexCatalog indexes_;

    static ServiceError mapIndexError(IndexError error) {
        switch (error) {
            case IndexError::NotBuilt: return ServiceError::IndexNotBuilt;
            case IndexError::Stale: return ServiceError::IndexStale;
            case IndexError::InvalidKey: return ServiceError::InvalidKey;
            case IndexError::InvalidRange: return ServiceError::InvalidRange;
        }
        return ServiceError::UnexpectedIndexError;
    }

    template <typename Predicate>
    DynamicArray<RecordId> findExactLinear(Predicate predicate) const {
        DynamicArray<RecordId> result;
        for (std::size_t position = 0; position < repository_.getCount(); ++position) {
            const Student& student = repository_.at(position);
            if (predicate(student)) result.push_back(student.id);
        }
        heapSort(result);
        return result;
    }

    template <typename Key, typename Extractor, typename Compare = std::less<Key>>
    std::expected<DynamicArray<RecordId>, ServiceError> findRangeLinear(
        const Key& lower,
        const Key& upper,
        Extractor extractor,
        Compare compare = {}
    ) const {
        if (compare(upper, lower)) return std::unexpected(ServiceError::InvalidRange);
        DynamicArray<IndexEntry<Key>> matches;
        for (std::size_t position = 0; position < repository_.getCount(); ++position) {
            const Student& student = repository_.at(position);
            const Key key = extractor(student);
            if (!compare(key, lower) && !compare(upper, key)) matches.push_back({key, student.id});
        }
        heapSort(matches, IndexEntryCompare<Key, Compare>{compare});
        DynamicArray<RecordId> result;
        for (const auto& match : matches) {
            result.push_back(match.recordId);
        }
        return result;
    }

    template <typename ExpectedResult>
    static std::expected<DynamicArray<RecordId>, ServiceError> mapIndexResult(ExpectedResult result) {
        if (!result) return std::unexpected(mapIndexError(result.error()));
        return std::move(result.value());
    }

public:
    const StudentRepository& getRepository() const noexcept { return repository_; }
    const StudentIndexCatalog& getIndexes() const noexcept { return indexes_; }

    std::expected<void, RepositoryError> addStudent(const Student& student) {
        return repository_.add(student);
    }

    std::expected<void, RepositoryError> replaceStudents(DynamicArray<Student> students) {
        return repository_.replaceAll(std::move(students));
    }

    void clearStudents() noexcept { repository_.clear(); }
    void buildIndexes() { indexes_.build(repository_); }

    std::expected<StudentIndexStatistics, IndexError> getIndexStatistics() const {
        return indexes_.getStatistics(repository_);
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findById(
        RecordId id,
        ExactSearchStrategy strategy
    ) const {
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear([id](const Student& student) { return student.id == id; });
        }
        if (strategy == ExactSearchStrategy::Binary) {
            return mapIndexResult(indexes_.findByIdBinary(repository_, id));
        }
        const auto result = indexes_.findPositionByIdTree(repository_, id);
        if (!result) return std::unexpected(mapIndexError(result.error()));
        DynamicArray<RecordId> ids;
        if (result->has_value()) ids.push_back(id);
        return ids;
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByLastName(
        const std::string& key,
        ExactSearchStrategy strategy
    ) const {
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear([&key](const Student& student) { return student.lastName == key; });
        }
        return strategy == ExactSearchStrategy::Tree
            ? mapIndexResult(indexes_.findByLastNameTree(repository_, key))
            : mapIndexResult(indexes_.findByLastNameBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByBirthYear(
        int key,
        ExactSearchStrategy strategy
    ) const {
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear([key](const Student& student) { return student.birthYear == key; });
        }
        return strategy == ExactSearchStrategy::Tree
            ? mapIndexResult(indexes_.findByBirthYearTree(repository_, key))
            : mapIndexResult(indexes_.findByBirthYearBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByGpa(
        double key,
        ExactSearchStrategy strategy
    ) const {
        if (!std::isfinite(key)) return std::unexpected(ServiceError::InvalidKey);
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear([key](const Student& student) { return student.gpa == key; });
        }
        return strategy == ExactSearchStrategy::Tree
            ? mapIndexResult(indexes_.findByGpaTree(repository_, key))
            : mapIndexResult(indexes_.findByGpaBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByCompositeKey(
        const StudentCompositeKey& key,
        ExactSearchStrategy strategy
    ) const {
        if (strategy == ExactSearchStrategy::Linear) {
            return findExactLinear([&key](const Student& student) {
                return student.lastName == key.lastName && student.firstName == key.firstName &&
                       student.birthYear == key.birthYear;
            });
        }
        return strategy == ExactSearchStrategy::Tree
            ? mapIndexResult(indexes_.findByCompositeKeyTree(repository_, key))
            : mapIndexResult(indexes_.findByCompositeKeyBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findInBirthYearRange(
        int lower,
        int upper,
        RangeSearchStrategy strategy
    ) const {
        if (strategy == RangeSearchStrategy::Linear) {
            return findRangeLinear<int>(lower, upper, [](const Student& student) {
                return student.birthYear;
            });
        }
        return mapIndexResult(indexes_.findInBirthYearRangeBinary(repository_, lower, upper));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findInGpaRange(
        double lower,
        double upper,
        RangeSearchStrategy strategy
    ) const {
        if (!std::isfinite(lower) || !std::isfinite(upper)) {
            return std::unexpected(ServiceError::InvalidKey);
        }
        if (strategy == RangeSearchStrategy::Linear) {
            return findRangeLinear<double>(lower, upper, [](const Student& student) {
                return student.gpa;
            });
        }
        return mapIndexResult(indexes_.findInGpaRangeBinary(repository_, lower, upper));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findInCompositeKeyRange(
        const StudentCompositeKey& lower,
        const StudentCompositeKey& upper,
        RangeSearchStrategy strategy
    ) const {
        if (strategy == RangeSearchStrategy::Linear) {
            return findRangeLinear<StudentCompositeKey>(
                lower,
                upper,
                [](const Student& student) {
                    return StudentCompositeKey{student.lastName, student.firstName, student.birthYear};
                },
                StudentCompositeKeyCompare{}
            );
        }
        return mapIndexResult(indexes_.findInCompositeKeyRangeBinary(repository_, lower, upper));
    }
};
