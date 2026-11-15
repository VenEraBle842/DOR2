#include "benchmark/benchmark_runner.hpp"

#include "dictionary/tree_dictionary.hpp"
#include "sequence/array_sorted_sequence.hpp"
#include "service/student_search_service.hpp"

#include <chrono>
#include <fstream>
#include <locale>
#include <random>
#include <string>
#include <system_error>

enum class InputOrder { Ascending, Descending, Random };

struct Measurement {
    double elapsedUs{0.0};
    std::size_t matchCount{0};
};

struct BatchResult {
    std::size_t matchCount{0};
    std::uint64_t checksum{0};
};

volatile std::uint64_t resultSink{0};

std::string orderName(InputOrder order) {
    switch (order) {
        case InputOrder::Ascending: return "Ascending";
        case InputOrder::Descending: return "Descending";
        case InputOrder::Random: return "Random";
    }
    return "Unknown";
}

DynamicArray<int> makeValues(std::size_t size, InputOrder order, std::uint64_t seed) {
    DynamicArray<int> values;
    for (std::size_t index = 0; index < size; ++index) {
        const std::size_t source = order == InputOrder::Descending ? size - index - 1 : index;
        values.push_back(static_cast<int>(source));
    }
    if (order == InputOrder::Random && size > 1) {
        std::mt19937_64 generator(seed);
        for (std::size_t remaining = size; remaining > 1; --remaining) {
            std::uniform_int_distribution<std::size_t> distribution(0, remaining - 1);
            const std::size_t selected = distribution(generator);
            const int temporary = values[remaining - 1];
            values[remaining - 1] = values[selected];
            values[selected] = temporary;
        }
    }
    return values;
}

DynamicArray<int> makeQueries(std::size_t size, std::size_t count, std::uint64_t seed) {
    DynamicArray<int> queries;
    std::mt19937_64 generator(seed + 17U);
    const int upper = static_cast<int>(size == 0 ? 1 : size - 1);
    std::uniform_int_distribution<int> hits(0, upper);
    for (std::size_t index = 0; index < count; ++index) {
        if (index % 2 == 0 && size > 0) {
            queries.push_back(hits(generator));
        } else {
            queries.push_back(static_cast<int>(size + index + 1));
        }
    }
    return queries;
}

DynamicArray<TwoElement<int, int>> makeRangeQueries(std::size_t size, std::size_t count) {
    DynamicArray<TwoElement<int, int>> queries;
    for (std::size_t index = 0; index < count; ++index) {
        if (index % 3 == 0) {
            queries.push_back({static_cast<int>(size + 1), static_cast<int>(size + 2)});
        } else if (index % 3 == 1) {
            const int lower = static_cast<int>(index % (size + 1));
            queries.push_back({lower, lower + 3});
        } else {
            queries.push_back({0, static_cast<int>(size)});
        }
    }
    return queries;
}

std::expected<StudentSearchService, RepositoryError> makeService(
    const DynamicArray<int>& values
) {
    DynamicArray<Student> students;
    for (const int key : values) {
        students.push_back(Student{
            static_cast<RecordId>(key) + 1, "First" + std::to_string(key % 23),
            "Last" + std::to_string(key % 17), key, 3.0 + static_cast<double>(key % 21) / 10.0
        });
    }
    StudentSearchService service;
    const auto replaced = service.replaceStudents(std::move(students));
    if (!replaced) return std::unexpected(replaced.error());
    service.buildIndexes();
    return service;
}

template <typename Action>
Measurement measureMedian(Action action) {
    constexpr std::size_t repetitions = 5;
    resultSink = action().checksum;
    ArraySortedSequence<double> samples;
    std::size_t matches = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        const BatchResult batch = action();
        const auto finish = std::chrono::steady_clock::now();
        resultSink = batch.checksum;
        matches = batch.matchCount;
        const auto elapsed = std::chrono::duration<double, std::micro>(finish - start).count();
        samples.add(elapsed);
    }
    return {samples.at(repetitions / 2), matches};
}

template <typename Structure, typename Builder>
std::expected<Measurement, BenchmarkFailure> measureBuild(Builder builder) {
    constexpr std::size_t repetitions = 5;
    {
        Structure warmup;
        const auto built = builder(warmup);
        if (!built) return std::unexpected(built.error());
        resultSink = *built;
    }
    ArraySortedSequence<double> samples;
    std::size_t count = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        Structure structure;
        const auto start = std::chrono::steady_clock::now();
        const auto built = builder(structure);
        const auto finish = std::chrono::steady_clock::now();
        if (!built) return std::unexpected(built.error());
        count = *built;
        resultSink = count;
        samples.add(std::chrono::duration<double, std::micro>(finish - start).count());
    }
    return Measurement{samples.at(repetitions / 2), count};
}

bool sameIds(const std::expected<DynamicArray<RecordId>, ServiceError>& left,
             const std::expected<DynamicArray<RecordId>, ServiceError>& right) {
    if (!left || !right || left->size() != right->size()) return false;
    for (std::size_t index = 0; index < left->size(); ++index) {
        if ((*left)[index] != (*right)[index]) return false;
    }
    return true;
}

bool verifyExact(const StudentSearchService& service, const DynamicArray<int>& queries) {
    for (const int query : queries) {
        const auto reference = service.findByBirthYear(query, SearchStrategy::Linear);
        if (!sameIds(reference, service.findByBirthYear(query, SearchStrategy::Tree)) ||
            !sameIds(reference, service.findByBirthYear(query, SearchStrategy::Binary))) return false;
    }
    return true;
}

bool verifyRanges(
    const StudentSearchService& service,
    const DynamicArray<TwoElement<int, int>>& queries
) {
    for (const auto& query : queries) {
        if (!sameIds(service.findBirthYearRange(query.first, query.second, RangeStrategy::Linear),
                     service.findBirthYearRange(query.first, query.second, RangeStrategy::Binary))) return false;
    }
    return true;
}

void consume(
    BatchResult& batch,
    const std::expected<DynamicArray<RecordId>, ServiceError>& ids
) {
    if (!ids) {
        batch.checksum ^= 0xffffffffffffffffULL;
        return;
    }
    batch.matchCount += ids->size();
    batch.checksum = (batch.checksum ^ ids->size()) * 1099511628211ULL;
    for (const RecordId id : *ids) batch.checksum = (batch.checksum ^ id) * 1099511628211ULL;
}

BatchResult exactBatch(
    const StudentSearchService& service,
    const DynamicArray<int>& queries,
    SearchStrategy strategy
) {
    BatchResult batch;
    for (const int query : queries) consume(batch, service.findByBirthYear(query, strategy));
    return batch;
}

BatchResult rangeBatch(
    const StudentSearchService& service,
    const DynamicArray<TwoElement<int, int>>& queries,
    RangeStrategy strategy
) {
    BatchResult batch;
    for (const auto& query : queries) {
        consume(batch, service.findBirthYearRange(query.first, query.second, strategy));
    }
    return batch;
}

void appendResult(
    DynamicArray<BenchmarkResult>& results,
    std::size_t size,
    const std::string& order,
    const std::string& algorithm,
    const std::string& operation,
    std::size_t queryCount,
    Measurement measurement,
    std::uint64_t seed,
    bool verified
) {
    results.push_back({
        size, order, algorithm, operation, queryCount, measurement.elapsedUs,
        measurement.matchCount, seed, verified
    });
}
std::expected<DynamicArray<BenchmarkResult>, BenchmarkFailure> BenchmarkRunner::run(
    std::size_t datasetSize,
    std::size_t queryCount,
    std::uint64_t seed
) {
    if (datasetSize > maxDatasetSize || queryCount > maxQueryCount ||
        (datasetSize != 0 && queryCount > maxWorkload / datasetSize)) {
        return std::unexpected(BenchmarkFailure{
            BenchmarkError::InvalidWorkload,
            "maximum dataset/query count is 10000; dataset size * query count must not exceed 10000000",
            std::nullopt
        });
    }
    DynamicArray<BenchmarkResult> results;
    for (int orderNumber = 0; orderNumber < 3; ++orderNumber) {
        const auto order = static_cast<InputOrder>(orderNumber);
        const DynamicArray<int> values = makeValues(datasetSize, order, seed);
        const DynamicArray<int> queries = makeQueries(datasetSize, queryCount, seed);
        const auto rangeQueries = makeRangeQueries(datasetSize, queryCount);
        const std::string name = orderName(order);

        const auto treeBuild = measureBuild<TreeDictionary<int, std::size_t>>(
            [&values](auto& tree) -> std::expected<std::size_t, BenchmarkFailure> {
                for (std::size_t index = 0; index < values.size(); ++index) {
                    const auto inserted = tree.add(values[index], index);
                    if (!inserted) {
                        return std::unexpected(BenchmarkFailure{
                            BenchmarkError::PreparationFailed,
                            "cannot build benchmark tree: duplicate key", std::nullopt
                        });
                    }
                }
                return tree.getCount();
            });
        if (!treeBuild) return std::unexpected(treeBuild.error());
        appendResult(results, datasetSize, name, "TreeDictionary", "Build", 1,
                     *treeBuild, seed, treeBuild->matchCount == datasetSize);

        const auto sortedBuild = measureBuild<ArraySortedSequence<int>>(
            [&values](auto& sequence) -> std::expected<std::size_t, BenchmarkFailure> {
                for (const int value : values) sequence.add(value);
                return sequence.getLength();
            });
        if (!sortedBuild) return std::unexpected(sortedBuild.error());
        appendResult(results, datasetSize, name, "ArraySortedSequence", "Build", 1,
                     *sortedBuild, seed, sortedBuild->matchCount == datasetSize);

        const auto prepared = makeService(values);
        if (!prepared) {
            return std::unexpected(BenchmarkFailure{
                BenchmarkError::PreparationFailed,
                "cannot prepare benchmark data: " + toString(prepared.error()), std::nullopt
            });
        }
        const StudentSearchService& service = *prepared;
        const bool exactVerified = verifyExact(service, queries);
        appendResult(results, datasetSize, name, "Linear", "ExactSearch", queryCount,
                     measureMedian([&]() { return exactBatch(service, queries, SearchStrategy::Linear); }),
                     seed, exactVerified);
        appendResult(results, datasetSize, name, "TreeDictionary", "ExactSearch", queryCount,
                     measureMedian([&]() { return exactBatch(service, queries, SearchStrategy::Tree); }),
                     seed, exactVerified);
        appendResult(results, datasetSize, name, "BinarySearch", "ExactSearch", queryCount,
                     measureMedian([&]() { return exactBatch(service, queries, SearchStrategy::Binary); }),
                     seed, exactVerified);

        const bool rangeVerified = verifyRanges(service, rangeQueries);
        appendResult(results, datasetSize, name, "Linear", "RangeSearch", queryCount,
                     measureMedian([&]() { return rangeBatch(service, rangeQueries, RangeStrategy::Linear); }),
                     seed, rangeVerified);
        appendResult(results, datasetSize, name, "BinarySearch", "RangeSearch", queryCount,
                     measureMedian([&]() { return rangeBatch(service, rangeQueries, RangeStrategy::Binary); }),
                     seed, rangeVerified);
    }
    return results;
}

std::expected<void, FileFailure> BenchmarkRunner::saveCsv(
    const std::filesystem::path& path,
    const DynamicArray<BenchmarkResult>& results
) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output.is_open()) {
        return std::unexpected(FileFailure{
            FileError::OpenFailed, "cannot open benchmark CSV", path, 0
        });
    }
    output << "DatasetSize,InputOrder,Algorithm,Operation,QueryCount,ElapsedUs,MatchCount,Seed,Verified\n";
    for (const BenchmarkResult& result : results) {
        output << result.datasetSize << ',' << result.inputOrder << ',' << result.algorithm << ','
               << result.operation << ',' << result.queryCount << ',' << result.elapsedUs << ','
               << result.matchCount << ',' << result.seed << ','
               << (result.verified ? "true" : "false") << '\n';
    }
    output.flush();
    if (!output) {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return std::unexpected(FileFailure{
            FileError::WriteFailed, "failed to write benchmark CSV", path, 0
        });
    }
    output.close();
    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);
    if (renameError) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return std::unexpected(FileFailure{
            FileError::WriteFailed, renameError.message(), path, 0
        });
    }
    return {};
}

std::expected<DynamicArray<BenchmarkResult>, BenchmarkFailure> BenchmarkRunner::runAndSave(
    const std::filesystem::path& path,
    std::size_t datasetSize,
    std::size_t queryCount,
    std::uint64_t seed
) {
    auto results = run(datasetSize, queryCount, seed);
    if (!results) return std::unexpected(results.error());
    if (auto saved = saveCsv(path, *results); !saved) {
        return std::unexpected(BenchmarkFailure{
            BenchmarkError::FileOperationFailed, saved.error().message, saved.error()
        });
    }
    return results;
}
