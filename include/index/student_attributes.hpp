#pragma once

#include "index/i_student_index.hpp"
#include "models/student_composite_key.hpp"

#include <functional>
#include <string>

using SearchKeyInput = DynamicArray<std::string>;

struct SearchAttributeDescription {
    SearchAttribute id{SearchAttribute::Id};
    std::string name;
    DynamicArray<std::string> fieldNames;
    bool supportsRange{false};
    std::string hint;
};

template <typename Key, typename Compare = std::less<Key>>
struct StudentAttribute {
    using KeyType = Key;

    SearchAttribute id;
    std::string name;
    std::function<Key(const Student&)> extractKey;
    DynamicArray<std::string> fieldNames{};
    std::function<std::expected<Key, Error>(const SearchKeyInput&)> parseKey{};
    bool supportsRange{false};
    std::string hint{};

    SearchAttributeDescription getDescription() const {
        return {id, name, fieldNames, supportsRange, hint};
    }
};

std::expected<RecordId, Error> parseIdKey(const SearchKeyInput& input);
std::expected<std::string, Error> parseLastNameKey(const SearchKeyInput& input);
std::expected<int, Error> parseBirthYearKey(const SearchKeyInput& input);
std::expected<double, Error> parseGpaKey(const SearchKeyInput& input);
std::expected<StudentCompositeKey, Error> parseCompositeKey(const SearchKeyInput& input);

inline const StudentAttribute<RecordId> studentIdAttribute{
    SearchAttribute::Id, "ID",
    [](const Student& student) { return student.id; },
    {"ID"}, parseIdKey, false
};

inline const StudentAttribute<std::string> studentLastNameAttribute{
    SearchAttribute::LastName, "Last name",
    [](const Student& student) { return student.lastName; },
    {"Last name"}, parseLastNameKey, false
};

inline const StudentAttribute<int> studentBirthYearAttribute{
    SearchAttribute::BirthYear, "Birth year",
    [](const Student& student) { return student.birthYear; },
    {"Birth year"}, parseBirthYearKey, true
};

inline const StudentAttribute<double> studentGpaAttribute{
    SearchAttribute::Gpa, "GPA",
    [](const Student& student) { return student.gpa; },
    {"GPA"}, parseGpaKey, true
};

inline const StudentAttribute<StudentCompositeKey, StudentCompositeKeyCompare> studentCompositeAttribute{
    SearchAttribute::CompositeKey, "Composite key",
    [](const Student& student) {
        return StudentCompositeKey{student.lastName, student.firstName, student.birthYear};
    },
    {"Last name", "First name", "Birth year"}, parseCompositeKey, true,
    "Keys are compared by last name, then first name, then birth year."
};

template <typename Action>
void forEachStudentAttribute(Action action) {
    action(studentIdAttribute);
    action(studentLastNameAttribute);
    action(studentBirthYearAttribute);
    action(studentGpaAttribute);
    action(studentCompositeAttribute);
}
