#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"
#include "common/heap_sort.hpp"
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
    inline static std::uint64_t nextSnapshotId_{0};

    static std::uint64_t newSnapshotId() noexcept {
        return ++nextSnapshotId_;
    }

    std::uint64_t snapshotId_{newSnapshotId()};

    void markChanged() noexcept {
        snapshotId_ = newSnapshotId();
    }

    void swap(StudentRepository& other) noexcept {
        using std::swap;
        swap(students_, other.students_);
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
        : students_(std::move(other.students_)),
          snapshotId_(std::exchange(other.snapshotId_, newSnapshotId())) {}

    StudentRepository& operator=(StudentRepository&& other) noexcept {
        if (this != &other) {
            StudentRepository moved(std::move(other));
            swap(moved);
        }
        return *this;
    }

    std::size_t getCount() const noexcept { return students_.size(); }
    bool isEmpty() const noexcept { return students_.empty(); }
    std::uint64_t getSnapshotId() const noexcept { return snapshotId_; }

    std::optional<Student> get(std::size_t position) const {
        if (position >= students_.size()) return std::nullopt;
        return students_[position];
    }

    const Student& at(std::size_t position) const { return students_.at(position); }

    std::optional<Student> findById(std::uint64_t id) const {
        for (const auto& student : students_) {
            if (student.id == id) return student;
        }
        return std::nullopt;
    }

    std::expected<void, Error> add(const Student& student) {
        if (!std::isfinite(student.gpa)) {
            return std::unexpected(makeError(ErrorCode::NonFiniteNumber,
                {.field = "GPA", .detail = "ID = " + std::to_string(student.id)}));
        }
        if (findById(student.id).has_value()) {
            return std::unexpected(makeError(ErrorCode::DuplicateId,
                {.field = "ID", .detail = "ID = " + std::to_string(student.id)}));
        }
        students_.push_back(student);
        markChanged();
        return {};
    }

    void clear() noexcept {
        students_.clear();
        markChanged();
    }

    std::expected<void, Error> replaceAll(DynamicArray<Student> replacement) {
        DynamicArray<std::uint64_t> ids(replacement.size());
        for (std::size_t position = 0; position < replacement.size(); ++position) {
            if (!std::isfinite(replacement[position].gpa)) {
                return std::unexpected(makeError(ErrorCode::NonFiniteNumber,
                    {.field = "GPA", .detail = "ID = " + std::to_string(replacement[position].id)}));
            }
            ids.push_back(replacement[position].id);
        }
        heapSort(ids);
        for (std::size_t i = 1; i < ids.size(); ++i) {
            if (ids[i - 1] == ids[i]) {
                return std::unexpected(makeError(ErrorCode::DuplicateId,
                    {.field = "ID", .detail = "ID = " + std::to_string(ids[i])}));
            }
        }
        students_ = std::move(replacement);
        markChanged();
        return {};
    }
};
