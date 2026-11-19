#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"
#include "models/student.hpp"
#include "storage/student_repository.hpp"

#include <expected>
#include <filesystem>

class StudentFileStorage {
public:
    static constexpr int formatVersion = 1;

    static std::expected<DynamicArray<Student>, Error> load(
        const std::filesystem::path& path
    );

    static std::expected<void, Error> save(
        const std::filesystem::path& path,
        const StudentRepository& repository
    );
};
