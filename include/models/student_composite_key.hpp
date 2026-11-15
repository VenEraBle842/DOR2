#pragma once

#include <string>
#include <utility>

struct StudentCompositeKey {
    std::string lastName;
    std::string firstName;
    int birthYear{0};

    StudentCompositeKey() = default;
    StudentCompositeKey(std::string last, std::string first, int year)
        : lastName(std::move(last)), firstName(std::move(first)), birthYear(year) {}

    friend bool operator==(const StudentCompositeKey&, const StudentCompositeKey&) = default;
};

struct StudentCompositeKeyCompare {
    bool operator()(const StudentCompositeKey& left, const StudentCompositeKey& right) const noexcept {
        if (left.lastName != right.lastName) return left.lastName < right.lastName;
        if (left.firstName != right.firstName) return left.firstName < right.firstName;
        return left.birthYear < right.birthYear;
    }
};
