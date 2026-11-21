#pragma once

#include "index/i_student_index.hpp"
#include "models/student_composite_key.hpp"

#include <functional>
#include <string>

template <typename Key, typename Compare = std::less<Key>>
struct StudentAttribute {
    using KeyType = Key;

    SearchAttribute id;
    std::string name;
    std::function<Key(const Student&)> extractKey;
};

inline const StudentAttribute<RecordId> studentIdAttribute{
    SearchAttribute::Id, "ID",
    [](const Student& student) { return student.id; }
};

inline const StudentAttribute<std::string> studentLastNameAttribute{
    SearchAttribute::LastName, "Last name",
    [](const Student& student) { return student.lastName; }
};

inline const StudentAttribute<int> studentBirthYearAttribute{
    SearchAttribute::BirthYear, "Birth year",
    [](const Student& student) { return student.birthYear; }
};

inline const StudentAttribute<double> studentGpaAttribute{
    SearchAttribute::Gpa, "GPA",
    [](const Student& student) { return student.gpa; }
};

inline const StudentAttribute<StudentCompositeKey, StudentCompositeKeyCompare> studentCompositeAttribute{
    SearchAttribute::CompositeKey, "Composite key",
    [](const Student& student) {
        return StudentCompositeKey{student.lastName, student.firstName, student.birthYear};
    }
};

template <typename Action>
void forEachStudentAttribute(Action action) {
    action(studentIdAttribute);
    action(studentLastNameAttribute);
    action(studentBirthYearAttribute);
    action(studentGpaAttribute);
    action(studentCompositeAttribute);
}
