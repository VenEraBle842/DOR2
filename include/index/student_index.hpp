#pragma once

#include "dictionary/i_dictionary.hpp"
#include "index/i_student_index.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename Key, typename Compare = std::less<Key>>
class StudentIndex final : public IStudentIndex<Key>, public IStudentRangeSearch<Key> {
private:
    using Dictionary = IDictionary<Key, StudentRecordReferences>;

    std::unique_ptr<Dictionary> dictionary_;
    std::function<Key(const Student&)> extractKey_;
    std::optional<std::uint64_t> snapshotId_;

    std::expected<void, Error> checkIndexState(const StudentRepository& repository) const {
        if (!snapshotId_) return std::unexpected(makeError(ErrorCode::IndexNotBuilt));
        if (*snapshotId_ != repository.getSnapshotId()) {
            return std::unexpected(makeError(ErrorCode::IndexStale));
        }
        return {};
    }

    static bool isValidKey(const Key& key) {
        if constexpr (std::is_floating_point_v<Key>) {
            return std::isfinite(key);
        } else {
            return true;
        }
    }

    static void insertReference(
        StudentRecordReferences& group,
        const StudentRecordReference& reference
    ) {
        std::size_t position = 0;
        while (position < group.size() && group[position].recordId < reference.recordId) {
            ++position;
        }
        group.push_back(reference);
        for (std::size_t index = group.size() - 1; index > position; --index) {
            group[index] = group[index - 1];
        }
        group[position] = reference;
    }

    static void addToGroup(
        Dictionary& dictionary,
        const Key& key,
        const StudentRecordReference& reference
    ) {
        const bool updated = dictionary.modify(key, [&reference](StudentRecordReferences& group) {
            insertReference(group, reference);
        });
        if (!updated) {
            StudentRecordReferences group;
            group.push_back(reference);
            const auto inserted = dictionary.add(key, group);
            if (!inserted) {
                throw std::logic_error("duplicate index group");
            }
        }
    }

public:
    StudentIndex(
        std::unique_ptr<Dictionary> dictionary,
        std::function<Key(const Student&)> extractor
    ) : dictionary_(std::move(dictionary)), extractKey_(std::move(extractor)) {
        if (!dictionary_ || !extractKey_) {
            throw std::invalid_argument("StudentIndex requires a dictionary and extractor");
        }
    }

    StudentIndex(const StudentIndex& other)
        : dictionary_(other.dictionary_->clone()), extractKey_(other.extractKey_),
          snapshotId_(other.snapshotId_) {}

    StudentIndex& operator=(const StudentIndex& other) {
        if (this != &other) {
            StudentIndex copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    StudentIndex(StudentIndex&& other) noexcept
        : dictionary_(std::move(other.dictionary_)), extractKey_(std::move(other.extractKey_)),
          snapshotId_(std::exchange(other.snapshotId_, std::nullopt)) {}

    StudentIndex& operator=(StudentIndex&& other) noexcept {
        if (this != &other) {
            dictionary_ = std::move(other.dictionary_);
            extractKey_ = std::move(other.extractKey_);
            snapshotId_ = std::exchange(other.snapshotId_, std::nullopt);
        }
        return *this;
    }

    ~StudentIndex() override = default;

    std::unique_ptr<IStudentIndexBase> clone() const override {
        return std::make_unique<StudentIndex>(*this);
    }

    std::unique_ptr<IStudentIndexBase> createEmpty() const override {
        return std::make_unique<StudentIndex>(dictionary_->createEmpty(), extractKey_);
    }

    void build(const StudentRepository& repository) override {
        if (!dictionary_) {
            throw std::logic_error("Cannot build a moved-from index before assignment");
        }
        auto replacement = dictionary_->createEmpty();
        for (std::size_t position = 0; position < repository.getCount(); ++position) {
            const Student& student = repository.at(position);
            const Key key = extractKey_(student);
            const StudentRecordReference reference{
                repository.getSnapshotId(), position, student.id
            };
            addToGroup(*replacement, key, reference);
        }
        dictionary_ = std::move(replacement);
        snapshotId_ = repository.getSnapshotId();
    }

    bool isBuilt() const noexcept override { return snapshotId_.has_value(); }

    bool isStale(const StudentRepository& repository) const noexcept override {
        return snapshotId_ && *snapshotId_ != repository.getSnapshotId();
    }

    std::size_t getKeyCount() const noexcept override {
        return dictionary_ ? dictionary_->getCount() : 0;
    }

    std::expected<StudentRecordReferences, Error> findExact(
        const StudentRepository& repository,
        const Key& key
    ) const override {
        if (!isValidKey(key)) return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        if (const auto state = checkIndexState(repository); !state) {
            return std::unexpected(state.error());
        }
        return dictionary_->find(key).value_or(StudentRecordReferences{});
    }

    bool supportsRangeSearch() const noexcept override {
        return dictionary_ && dictionary_->supportsRangeSearch();
    }

    const IStudentRangeSearch<Key>& rangeSearch() const override {
        if (!supportsRangeSearch()) {
            throw std::logic_error("Range search is not supported by this index");
        }
        return *this;
    }

    std::expected<StudentRecordReferences, Error> findRange(
        const StudentRepository& repository,
        const Key& lower,
        const Key& upper
    ) const override {
        if (!isValidKey(lower) || !isValidKey(upper)) {
            return std::unexpected(makeError(ErrorCode::NonFiniteNumber));
        }
        if (const auto state = checkIndexState(repository); !state) {
            return std::unexpected(state.error());
        }
        if (Compare{}(upper, lower)) return std::unexpected(makeError(ErrorCode::InvalidRange));
        if (!dictionary_->supportsRangeSearch()) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range strategy"}));
        }
        const auto& range = dictionary_->rangeSearch();
        StudentRecordReferences result;
        const auto visited = range.visitRange(lower, upper,
            [&result](const Key&, const StudentRecordReferences& group) {
                for (const auto& reference : group) {
                    result.push_back(reference);
                }
            });
        if (!visited) return std::unexpected(visited.error());
        return result;
    }
};
