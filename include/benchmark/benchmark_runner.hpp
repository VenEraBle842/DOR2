#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

struct BenchmarkResult {
    std::size_t datasetSize{0};
    std::string inputOrder;
    std::string algorithm;
    std::string operation;
    std::size_t queryCount{0};
    double elapsedUs{0.0};
    std::size_t matchCount{0};
    std::uint64_t seed{0};
    bool verified{false};
};

struct BenchmarkFailure {
    BenchmarkError code;
    std::string message;
    std::optional<FileFailure> fileFailure;
};

inline AppError makeError(const BenchmarkFailure& failure) {
    if (failure.fileFailure) return makeError(*failure.fileFailure);
    return makeError(failure.code, {.detail = failure.message});
}

class BenchmarkRunner {
public:
    static constexpr std::size_t maxDatasetSize = 10'000;
    static constexpr std::size_t maxQueryCount = 10'000;
    static constexpr std::size_t maxWorkload = 10'000'000;

    static std::expected<DynamicArray<BenchmarkResult>, BenchmarkFailure> run(
        std::size_t datasetSize,
        std::size_t queryCount,
        std::uint64_t seed
    );

    static std::expected<void, FileFailure> saveCsv(
        const std::filesystem::path& path,
        const DynamicArray<BenchmarkResult>& results
    );

    static std::expected<DynamicArray<BenchmarkResult>, BenchmarkFailure> runAndSave(
        const std::filesystem::path& path,
        std::size_t datasetSize,
        std::size_t queryCount,
        std::uint64_t seed
    );
};
