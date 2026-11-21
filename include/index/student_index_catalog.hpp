#pragma once

#include "dictionary/sorted_array_dictionary.hpp"
#include "dictionary/tree_dictionary.hpp"
#include "index/student_attributes.hpp"
#include "index/student_index.hpp"

struct StudentIndexStatistics {
    std::size_t recordCount{0};
    std::size_t lastNameKeyCount{0};
    std::size_t birthYearKeyCount{0};
    std::size_t gpaKeyCount{0};
    std::size_t compositeKeyCount{0};
};

struct StudentIndexState {
    IndexSelection selection;
    std::string name;
    bool built{false};
    bool stale{false};
    std::size_t keyCount{0};
};

class StudentIndexCatalog {
private:
    struct Registration {
        IndexSelection selection;
        std::string name;
        std::shared_ptr<IStudentIndexBase> index;
    };

    DynamicArray<Registration> indexes_;

    std::size_t getTreeKeyCount(SearchAttribute attribute) const;

public:
    StudentIndexCatalog();
    StudentIndexCatalog(const StudentIndexCatalog& other);
    StudentIndexCatalog& operator=(const StudentIndexCatalog& other);
    StudentIndexCatalog(StudentIndexCatalog&&) noexcept = default;
    StudentIndexCatalog& operator=(StudentIndexCatalog&&) noexcept = default;

    template <typename Key, typename Compare>
    void registerIndex(const StudentAttribute<Key, Compare>& attribute, IndexBackend backend) {
        const IndexSelection selection{attribute.id, backend};
        if (getIndex(selection)) {
            throw std::invalid_argument("duplicate index registration");
        }
        std::unique_ptr<IDictionary<Key, StudentRecordReferences>> dictionary;
        if (backend == IndexBackend::Tree) {
            dictionary = std::make_unique<TreeDictionary<Key, StudentRecordReferences, Compare>>();
        } else if (backend == IndexBackend::SortedArray) {
            dictionary = std::make_unique<SortedArrayDictionary<Key, StudentRecordReferences, Compare>>();
        } else {
            throw std::invalid_argument("unknown index backend");
        }
        auto index = std::make_shared<StudentIndex<Key, Compare>>(
            std::move(dictionary), attribute.extractKey
        );
        indexes_.push_back({selection, attribute.name, std::move(index)});
    }

    std::shared_ptr<const IStudentIndexBase> getIndex(IndexSelection selection) const noexcept;

    template <typename TKey>
    std::shared_ptr<const IStudentIndex<TKey>> getIndex(IndexSelection selection) const noexcept {
        const auto index = getIndex(selection);
        return std::dynamic_pointer_cast<const IStudentIndex<TKey>>(index);
    }

    DynamicArray<IndexSelection> getSelections() const;
    DynamicArray<StudentIndexState> getStates(const StudentRepository& repository) const;

    std::expected<void, Error> build(
        const StudentRepository& repository,
        const DynamicArray<IndexSelection>& selections
    );

    void build(const StudentRepository& repository);
    bool isBuilt() const noexcept;
    bool isStale(const StudentRepository& repository) const noexcept;

    std::expected<StudentIndexStatistics, Error> getStatistics(
        const StudentRepository& repository
    ) const;
};
