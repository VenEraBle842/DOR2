#include "index/student_index_catalog.hpp"

void StudentIndexCatalog::build(const StudentRepository& repository) {
    idTreeIndex_.clear();
    lastNameTreeIndex_.clear();
    birthYearTreeIndex_.clear();
    gpaTreeIndex_.clear();
    compositeTreeIndex_.clear();
    idSortedArrayIndex_.clear();
    lastNameSortedArrayIndex_.clear();
    birthYearSortedArrayIndex_.clear();
    gpaSortedArrayIndex_.clear();
    compositeSortedArrayIndex_.clear();
    indexedSnapshotId_.reset();

    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        const Student& student = repository.at(position);
        const StudentCompositeKey composite{
            student.lastName, student.firstName, student.birthYear
        };

        const auto inserted = idTreeIndex_.add(student.id, position);
        if (!inserted) {
            throw std::logic_error("duplicate student ID while building indexes");
        }
        idSortedArrayIndex_.add({student.id, student.id});
        addToGroup(lastNameTreeIndex_, student.lastName, student.id);
        addToGroup(birthYearTreeIndex_, student.birthYear, student.id);
        addToGroup(gpaTreeIndex_, student.gpa, student.id);
        addToGroup(compositeTreeIndex_, composite, student.id);

        lastNameSortedArrayIndex_.add({student.lastName, student.id});
        birthYearSortedArrayIndex_.add({student.birthYear, student.id});
        gpaSortedArrayIndex_.add({student.gpa, student.id});
        compositeSortedArrayIndex_.add({composite, student.id});
    }

    indexedSnapshotId_ = repository.getSnapshotId();
}

std::expected<StudentIndexStatistics, Error> StudentIndexCatalog::getStatistics(
    const StudentRepository& repository
) const {
    if (const auto state = checkIndexState(repository); !state) return std::unexpected(state.error());
    return StudentIndexStatistics{
        repository.getCount(),
        lastNameTreeIndex_.getCount(),
        birthYearTreeIndex_.getCount(),
        gpaTreeIndex_.getCount(),
        compositeTreeIndex_.getCount()
    };
}

std::expected<std::optional<std::size_t>, Error> StudentIndexCatalog::findPositionByIdTree(
    const StudentRepository& repository,
    RecordId id
) const {
    if (const auto state = checkIndexState(repository); !state) return std::unexpected(state.error());
    const auto position = idTreeIndex_.find(id);
    if (!position) return std::optional<std::size_t>{};
    return position;
}
