#include "index/student_index_catalog.hpp"

void StudentIndexCatalog::build(const StudentRepository& repository) {
    idIndex_.clear();
    lastNameIndex_.clear();
    birthYearIndex_.clear();
    gpaIndex_.clear();
    compositeIndex_.clear();
    idSequence_.clear();
    lastNameSequence_.clear();
    birthYearSequence_.clear();
    gpaSequence_.clear();
    compositeSequence_.clear();
    builtSnapshotId_.reset();

    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        const Student& student = repository.at(position);
        const StudentCompositeKey composite{
            student.lastName, student.firstName, student.birthYear
        };

        const auto inserted = idIndex_.add(student.id, position);
        if (!inserted) {
            throw std::logic_error("duplicate student ID while building indexes");
        }
        idSequence_.add({student.id, student.id});
        addToGroup(lastNameIndex_, student.lastName, student.id);
        addToGroup(birthYearIndex_, student.birthYear, student.id);
        addToGroup(gpaIndex_, student.gpa, student.id);
        addToGroup(compositeIndex_, composite, student.id);

        lastNameSequence_.add({student.lastName, student.id});
        birthYearSequence_.add({student.birthYear, student.id});
        gpaSequence_.add({student.gpa, student.id});
        compositeSequence_.add({composite, student.id});
    }

    builtSnapshotId_ = repository.getSnapshotId();
}

std::expected<StudentIndexStatistics, IndexError> StudentIndexCatalog::getStatistics(
    const StudentRepository& repository
) const {
    if (const auto state = verifyState(repository); !state) return std::unexpected(state.error());
    return StudentIndexStatistics{
        repository.getCount(),
        lastNameIndex_.getCount(),
        birthYearIndex_.getCount(),
        gpaIndex_.getCount(),
        compositeIndex_.getCount()
    };
}

std::expected<std::optional<std::size_t>, IndexError> StudentIndexCatalog::findId(
    const StudentRepository& repository,
    RecordId id
) const {
    if (const auto state = verifyState(repository); !state) return std::unexpected(state.error());
    const auto position = idIndex_.find(id);
    if (!position) return std::optional<std::size_t>{};
    return position;
}
