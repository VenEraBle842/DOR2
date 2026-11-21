#pragma once

#include "index/student_record_reference.hpp"
#include "storage/student_repository.hpp"

#include <expected>
#include <memory>

enum class SearchAttribute { Id, LastName, BirthYear, Gpa, CompositeKey };
enum class IndexBackend { Tree, SortedArray };
enum class ExactSearchStrategy { Linear, Tree, Binary };
enum class RangeSearchStrategy { Linear, Binary };

struct IndexSelection {
    SearchAttribute attribute{SearchAttribute::Id};
    IndexBackend backend{IndexBackend::Tree};
    friend bool operator==(const IndexSelection&, const IndexSelection&) = default;
};

class IStudentIndexBase {
public:
    virtual ~IStudentIndexBase() = default;

    virtual std::unique_ptr<IStudentIndexBase> clone() const = 0;
    virtual std::unique_ptr<IStudentIndexBase> createEmpty() const = 0;
    virtual void build(const StudentRepository& repository) = 0;
    virtual bool isBuilt() const noexcept = 0;
    virtual bool isStale(const StudentRepository& repository) const noexcept = 0;
    virtual std::size_t getKeyCount() const noexcept = 0;
    virtual bool supportsRangeSearch() const noexcept = 0;
};

template <typename Key>
class IStudentExactSearch {
public:
    virtual ~IStudentExactSearch() = default;

    virtual std::expected<StudentRecordReferences, Error> findExact(
        const StudentRepository& repository,
        const Key& key
    ) const = 0;
};

template <typename Key>
class IStudentRangeSearch {
public:
    virtual ~IStudentRangeSearch() = default;

    virtual std::expected<StudentRecordReferences, Error> findRange(
        const StudentRepository& repository,
        const Key& lower,
        const Key& upper
    ) const = 0;
};

template <typename Key>
class IStudentIndex : public IStudentIndexBase, public IStudentExactSearch<Key> {
public:
    ~IStudentIndex() override = default;

    virtual const IStudentRangeSearch<Key>& getRangeSearch() const = 0;
};
