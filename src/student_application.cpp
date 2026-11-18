#include "application/student_application.hpp"
#include "storage/student_file_storage.hpp"

#include <random>
#include <utility>

std::expected<void, AppError> StudentApplication::addStudent(const Student& student) {
    if (const auto added = searchService_.addStudent(student); !added) {
        return std::unexpected(makeError(added.error(), {.detail = "ID = " + std::to_string(student.id)}));
    }
    return {};
}

std::expected<void, AppError> StudentApplication::generateStudents(std::size_t count, std::uint64_t seed) {
    if (count > maxGeneratedRecords) {
        return std::unexpected(makeError(AppErrorCode::LimitExceeded,
            {.field = "Record count", .detail = "Maximum: " + std::to_string(maxGeneratedRecords)}));
    }
    DynamicArray<Student> students;
    std::mt19937_64 generator(seed);
    for (std::size_t index = 0; index < count; ++index) {
        const int year = 1998 + static_cast<int>(generator() % 10U);
        const double gpa = 3.0 + static_cast<double>(generator() % 21U) / 10.0;
        students.push_back({1000U + index, "First" + std::to_string(generator() % 23U),
                            "Last" + std::to_string(generator() % 17U), year, gpa});
    }
    if (const auto replaced = searchService_.replaceStudents(std::move(students)); !replaced) {
        return std::unexpected(makeError(replaced.error()));
    }
    return {};
}

std::expected<void, AppError> StudentApplication::loadStudents(const std::filesystem::path& path) {
    auto loaded = StudentFileStorage::load(path);
    if (!loaded) return std::unexpected(makeError(loaded.error()));
    StudentSearchService replacement;
    if (const auto replaced = replacement.replaceStudents(std::move(*loaded)); !replaced) {
        return std::unexpected(makeError(replaced.error(), {.path = path}));
    }
    replacement.buildIndexes();
    searchService_ = std::move(replacement);
    return {};
}

std::expected<void, AppError> StudentApplication::saveStudents(const std::filesystem::path& path) const {
    if (const auto saved = StudentFileStorage::save(path, searchService_.getRepository()); !saved) {
        return std::unexpected(makeError(saved.error()));
    }
    return {};
}

std::expected<void, AppError> StudentApplication::buildIndexes() {
    searchService_.buildIndexes();
    return {};
}

std::expected<void, AppError> StudentApplication::runBenchmark(
    std::size_t size, std::size_t queries, std::uint64_t seed
) {
    auto results = BenchmarkRunner::run(size, queries, seed);
    if (!results) return std::unexpected(makeError(results.error()));
    latestResults_ = std::move(*results);
    return {};
}

std::expected<void, AppError> StudentApplication::exportBenchmark(const std::filesystem::path& path) const {
    if (latestResults_.empty()) return std::unexpected(makeError(AppErrorCode::NoBenchmarkResults));
    if (const auto saved = BenchmarkRunner::saveCsv(path, latestResults_); !saved) {
        return std::unexpected(makeError(saved.error()));
    }
    return {};
}
