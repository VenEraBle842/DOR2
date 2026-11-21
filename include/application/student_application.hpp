#pragma once

#include "benchmark/benchmark_runner.hpp"
#include "application/search_request.hpp"

#include <expected>
#include <filesystem>

class StudentApplication {
private:
    StudentSearchService searchService_;
    DynamicArray<BenchmarkResult> latestResults_;

public:
    static constexpr std::size_t maxGeneratedRecords = 10'000;

    const StudentSearchService& getSearchService() const noexcept { return searchService_; }
    const DynamicArray<BenchmarkResult>& getLatestResults() const noexcept { return latestResults_; }

    std::expected<DynamicArray<RecordId>, Error> searchIds(const SearchRequest& request) const {
        return StudentSearchService::recordIds(searchReferences(request));
    }

    std::expected<StudentRecordReferences, Error> searchReferences(const SearchRequest& request) const {
        return executeSearch(searchService_, request);
    }

    std::expected<void, Error> buildIndexes(const DynamicArray<IndexSelection>& selections) {
        return searchService_.buildIndexes(selections);
    }

    std::expected<void, Error> addStudent(const Student& student);
    std::expected<void, Error> generateStudents(std::size_t count, std::uint64_t seed);
    std::expected<void, Error> loadStudents(const std::filesystem::path& path);
    std::expected<void, Error> saveStudents(const std::filesystem::path& path) const;
    std::expected<void, Error> buildIndexes();
    std::expected<void, Error> runBenchmark(std::size_t size, std::size_t queries, std::uint64_t seed);
    std::expected<void, Error> exportBenchmark(const std::filesystem::path& path) const;
};
