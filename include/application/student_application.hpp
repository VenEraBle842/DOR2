#pragma once

#include "benchmark/benchmark_runner.hpp"
#include "application/search_request.hpp"

#include <expected>
#include <filesystem>

class StudentApplication {
    StudentSearchService searchService_;
    DynamicArray<BenchmarkResult> latestResults_;

public:
    static constexpr std::size_t maxGeneratedRecords = 10'000;

    const StudentSearchService& getSearchService() const noexcept { return searchService_; }
    const DynamicArray<BenchmarkResult>& getLatestResults() const noexcept { return latestResults_; }

    std::expected<DynamicArray<RecordId>, AppError> search(const SearchRequest& request) const {
        return executeSearch(searchService_, request);
    }

    std::expected<void, AppError> addStudent(const Student& student);
    std::expected<void, AppError> generateStudents(std::size_t count, std::uint64_t seed);
    std::expected<void, AppError> loadStudents(const std::filesystem::path& path);
    std::expected<void, AppError> saveStudents(const std::filesystem::path& path) const;
    std::expected<void, AppError> buildIndexes();
    std::expected<void, AppError> runBenchmark(std::size_t size, std::size_t queries, std::uint64_t seed);
    std::expected<void, AppError> exportBenchmark(const std::filesystem::path& path) const;
};
