#pragma once

#include "common/errors.hpp"
#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
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

class BenchmarkRunner {
public:
    static constexpr std::size_t maxDatasetSize = 10'000;
    static constexpr std::size_t maxQueryCount = 10'000;
    static constexpr std::size_t maxWorkload = 10'000'000;

    static std::expected<DynamicArray<BenchmarkResult>, Error> run(
        std::size_t datasetSize,
        std::size_t queryCount,
        std::uint64_t seed
    );

    static std::expected<void, Error> saveCsv(
        const std::filesystem::path& path,
        const DynamicArray<BenchmarkResult>& results
    );

    static std::expected<DynamicArray<BenchmarkResult>, Error> runAndSave(
        const std::filesystem::path& path,
        std::size_t datasetSize,
        std::size_t queryCount,
        std::uint64_t seed
    );
};
