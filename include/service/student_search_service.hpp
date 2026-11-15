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

enum class SearchStrategy { Linear, Tree, Binary };
enum class RangeStrategy { Linear, Binary };

enum class ServiceError {
    IndexNotBuilt,
    IndexStale,
    InvalidRange,
    InvalidKey,
    UnsupportedStrategy,
    VerificationFailed
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
        }
        return ServiceError::VerificationFailed;
    }

    template <typename Predicate>
    DynamicArray<RecordId> linearExact(Predicate predicate) const {
        DynamicArray<RecordId> result;
        for (std::size_t position = 0; position < repository_.getCount(); ++position) {
            const Student& student = repository_.at(position);
            if (predicate(student)) result.push_back(student.id);
        }
        heapSort(result);
        return result;
    }

    template <typename Key, typename Extractor, typename Compare = std::less<Key>>
    std::expected<DynamicArray<RecordId>, ServiceError> linearRange(
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

    template <typename ExpectedResult>
    static std::expected<DynamicArray<RecordId>, ServiceError> mapRangeResult(ExpectedResult result) {
        if (!result) {
            if (result.error() == SearchError::InvalidKey) {
                return std::unexpected(ServiceError::InvalidKey);
            }
            return std::unexpected(
                result.error() == SearchError::InvalidRange
                    ? ServiceError::InvalidRange
                    : ServiceError::VerificationFailed
            );
        }
        return std::move(result.value());
    }

    static bool equalIds(const DynamicArray<RecordId>& left, const DynamicArray<RecordId>& right) {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (left[index] != right[index]) return false;
        }
        return true;
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
        SearchStrategy strategy
    ) const {
        if (strategy == SearchStrategy::Linear) {
            return linearExact([id](const Student& student) { return student.id == id; });
        }
        if (strategy == SearchStrategy::Binary) {
            return mapIndexResult(indexes_.findIdBinary(repository_, id));
        }
        const auto result = indexes_.findId(repository_, id);
        if (!result) return std::unexpected(mapIndexError(result.error()));
        DynamicArray<RecordId> ids;
        if (result->has_value()) ids.push_back(id);
        return ids;
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByLastName(
        const std::string& key,
        SearchStrategy strategy
    ) const {
        if (strategy == SearchStrategy::Linear) {
            return linearExact([&key](const Student& student) { return student.lastName == key; });
        }
        return strategy == SearchStrategy::Tree
            ? mapIndexResult(indexes_.findLastNameTree(repository_, key))
            : mapIndexResult(indexes_.findLastNameBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByBirthYear(
        int key,
        SearchStrategy strategy
    ) const {
        if (strategy == SearchStrategy::Linear) {
            return linearExact([key](const Student& student) { return student.birthYear == key; });
        }
        return strategy == SearchStrategy::Tree
            ? mapIndexResult(indexes_.findBirthYearTree(repository_, key))
            : mapIndexResult(indexes_.findBirthYearBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByGpa(
        double key,
        SearchStrategy strategy
    ) const {
        if (!std::isfinite(key)) return std::unexpected(ServiceError::InvalidKey);
        if (strategy == SearchStrategy::Linear) {
            return linearExact([key](const Student& student) { return student.gpa == key; });
        }
        return strategy == SearchStrategy::Tree
            ? mapIndexResult(indexes_.findGpaTree(repository_, key))
            : mapIndexResult(indexes_.findGpaBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findByComposite(
        const StudentCompositeKey& key,
        SearchStrategy strategy
    ) const {
        if (strategy == SearchStrategy::Linear) {
            return linearExact([&key](const Student& student) {
                return student.lastName == key.lastName && student.firstName == key.firstName &&
                       student.birthYear == key.birthYear;
            });
        }
        return strategy == SearchStrategy::Tree
            ? mapIndexResult(indexes_.findCompositeTree(repository_, key))
            : mapIndexResult(indexes_.findCompositeBinary(repository_, key));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findBirthYearRange(
        int lower,
        int upper,
        RangeStrategy strategy
    ) const {
        if (strategy == RangeStrategy::Linear) {
            return linearRange<int>(lower, upper, [](const Student& student) {
                return student.birthYear;
            });
        }
        if (!indexes_.getIsBuilt()) return std::unexpected(ServiceError::IndexNotBuilt);
        if (indexes_.getIsStale(repository_)) return std::unexpected(ServiceError::IndexStale);
        return mapRangeResult(indexes_.findBirthYearRange(repository_, lower, upper));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findGpaRange(
        double lower,
        double upper,
        RangeStrategy strategy
    ) const {
        if (!std::isfinite(lower) || !std::isfinite(upper)) {
            return std::unexpected(ServiceError::InvalidKey);
        }
        if (strategy == RangeStrategy::Linear) {
            return linearRange<double>(lower, upper, [](const Student& student) {
                return student.gpa;
            });
        }
        if (!indexes_.getIsBuilt()) return std::unexpected(ServiceError::IndexNotBuilt);
        if (indexes_.getIsStale(repository_)) return std::unexpected(ServiceError::IndexStale);
        return mapRangeResult(indexes_.findGpaRange(repository_, lower, upper));
    }

    std::expected<DynamicArray<RecordId>, ServiceError> findCompositeRange(
        const StudentCompositeKey& lower,
        const StudentCompositeKey& upper,
        RangeStrategy strategy
    ) const {
        if (strategy == RangeStrategy::Linear) {
            return linearRange<StudentCompositeKey>(
                lower,
                upper,
                [](const Student& student) {
                    return StudentCompositeKey{student.lastName, student.firstName, student.birthYear};
                },
                StudentCompositeKeyCompare{}
            );
        }
        if (!indexes_.getIsBuilt()) return std::unexpected(ServiceError::IndexNotBuilt);
        if (indexes_.getIsStale(repository_)) return std::unexpected(ServiceError::IndexStale);
        return mapRangeResult(indexes_.findCompositeRange(repository_, lower, upper));
    }

    std::expected<void, ServiceError> verifyLastName(const std::string& key) const {
        const auto linear = findByLastName(key, SearchStrategy::Linear);
        const auto tree = findByLastName(key, SearchStrategy::Tree);
        const auto binary = findByLastName(key, SearchStrategy::Binary);
        if (!tree) return std::unexpected(tree.error());
        if (!binary) return std::unexpected(binary.error());
        if (!equalIds(*linear, *tree) || !equalIds(*linear, *binary)) {
            return std::unexpected(ServiceError::VerificationFailed);
        }
        return {};
    }

    std::expected<void, ServiceError> verifyBirthYearRange(int lower, int upper) const {
        const auto linear = findBirthYearRange(lower, upper, RangeStrategy::Linear);
        const auto binary = findBirthYearRange(lower, upper, RangeStrategy::Binary);
        if (!linear) return std::unexpected(linear.error());
        if (!binary) return std::unexpected(binary.error());
        if (!equalIds(*linear, *binary)) return std::unexpected(ServiceError::VerificationFailed);
        return {};
    }
};
