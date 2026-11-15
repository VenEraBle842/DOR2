#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"
#include "dictionary/tree_dictionary.hpp"
#include "models/student.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>

class StudentRepository {
private:
    DynamicArray<Student> students_;
    TreeDictionary<std::uint64_t, std::size_t> positionsById_;
    std::size_t version_{0};
    inline static std::uint64_t nextSnapshotId_{0};

    static std::uint64_t newSnapshotId() noexcept {
        return ++nextSnapshotId_;
    }

    std::uint64_t snapshotId_{newSnapshotId()};

    void markChanged() noexcept {
        ++version_;
        snapshotId_ = newSnapshotId();
    }

    void swap(StudentRepository& other) noexcept {
        using std::swap;
        swap(students_, other.students_);
        swap(positionsById_, other.positionsById_);
        swap(version_, other.version_);
        swap(snapshotId_, other.snapshotId_);
    }

public:
    StudentRepository() = default;
    StudentRepository(const StudentRepository&) = default;

    StudentRepository& operator=(const StudentRepository& other) {
        if (this != &other) {
            StudentRepository copy(other);
            swap(copy);
        }
        return *this;
    }

    StudentRepository(StudentRepository&& other) noexcept
        : students_(std::move(other.students_)), positionsById_(std::move(other.positionsById_)),
          version_(std::exchange(other.version_, 0)),
          snapshotId_(std::exchange(other.snapshotId_, newSnapshotId())) {}

    StudentRepository& operator=(StudentRepository&& other) noexcept {
        if (this != &other) {
            StudentRepository moved(std::move(other));
            swap(moved);
        }
        return *this;
    }

    std::size_t getCount() const noexcept { return students_.size(); }
    bool getIsEmpty() const noexcept { return students_.empty(); }
    std::size_t getVersion() const noexcept { return version_; }
    std::uint64_t getSnapshotId() const noexcept { return snapshotId_; }

    std::optional<Student> get(std::size_t position) const {
        if (position >= students_.size()) return std::nullopt;
        return students_[position];
    }

    const Student& at(std::size_t position) const { return students_.at(position); }

    std::optional<Student> findById(std::uint64_t id) const {
        const auto position = positionsById_.find(id);
        if (!position) return std::nullopt;
        return students_[*position];
    }

    std::expected<void, RepositoryError> add(const Student& student) {
        if (!std::isfinite(student.gpa)) return std::unexpected(RepositoryError::InvalidGpa);
        if (positionsById_.containsKey(student.id)) {
            return std::unexpected(RepositoryError::DuplicateId);
        }
        students_.push_back(student);
        const auto inserted = positionsById_.add(student.id, students_.size() - 1);
        if (!inserted) {
            students_.pop_back();
            return std::unexpected(RepositoryError::DuplicateId);
        }
        markChanged();
        return {};
    }

    void clear() noexcept {
        students_.clear();
        positionsById_.clear();
        markChanged();
    }

    std::expected<void, RepositoryError> replaceAll(DynamicArray<Student> replacement) {
        TreeDictionary<std::uint64_t, std::size_t> newPositions;
        for (std::size_t position = 0; position < replacement.size(); ++position) {
            if (!std::isfinite(replacement[position].gpa)) {
                return std::unexpected(RepositoryError::InvalidGpa);
            }
            if (!newPositions.add(replacement[position].id, position)) {
                return std::unexpected(RepositoryError::DuplicateId);
            }
        }
        students_ = std::move(replacement);
        positionsById_ = std::move(newPositions);
        markChanged();
        return {};
    }
};
