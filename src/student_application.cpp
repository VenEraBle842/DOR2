#include "application/student_application.hpp"
#include "storage/student_file_storage.hpp"

#include <random>
#include <utility>

std::expected<void, Error> StudentApplication::addStudent(const Student& student) {
    return searchService_.addStudent(student);
}

std::expected<void, Error> StudentApplication::generateStudents(std::size_t count, std::uint64_t seed) {
    if (count > maxGeneratedRecords) {
        return std::unexpected(makeError(ErrorCode::LimitExceeded,
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
        return std::unexpected(replaced.error());
    }
    return {};
}

std::expected<void, Error> StudentApplication::loadStudents(const std::filesystem::path& path) {
    auto loaded = StudentFileStorage::load(path);
    if (!loaded) return std::unexpected(loaded.error());
    StudentSearchService replacement;
    if (const auto replaced = replacement.replaceStudents(std::move(*loaded)); !replaced) {
        Error error = replaced.error();
        error.context.path = path;
        return std::unexpected(std::move(error));
    }
    replacement.buildIndexes();
    searchService_ = std::move(replacement);
    return {};
}

std::expected<void, Error> StudentApplication::saveStudents(const std::filesystem::path& path) const {
    if (const auto saved = StudentFileStorage::save(path, searchService_.getRepository()); !saved) {
        return std::unexpected(saved.error());
    }
    return {};
}

std::expected<void, Error> StudentApplication::buildIndexes() {
    searchService_.buildIndexes();
    return {};
}

std::expected<void, Error> StudentApplication::runBenchmark(
    std::size_t size, std::size_t queries, std::uint64_t seed
) {
    auto results = BenchmarkRunner::run(size, queries, seed);
    if (!results) return std::unexpected(results.error());
    latestResults_ = std::move(*results);
    return {};
}

std::expected<void, Error> StudentApplication::exportBenchmark(const std::filesystem::path& path) const {
    if (latestResults_.empty()) return std::unexpected(makeError(ErrorCode::NoBenchmarkResults));
    if (const auto saved = BenchmarkRunner::saveCsv(path, latestResults_); !saved) {
        return std::unexpected(saved.error());
    }
    return {};
}
