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

volatile std::uint64_t benchmarkChecksumSink{0};

std::string orderName(InputOrder order) {
    switch (order) {
        case InputOrder::Ascending: return "Ascending";
        case InputOrder::Descending: return "Descending";
        case InputOrder::Random: return "Random";
    }
    return "Unknown";
}

DynamicArray<int> makeInputKeys(std::size_t size, InputOrder order, std::uint64_t seed) {
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

DynamicArray<int> makeExactQueries(std::size_t size, std::size_t count, std::uint64_t seed) {
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

std::expected<StudentSearchService, Error> makeBenchmarkService(
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
    const auto built = service.buildIndexes({
        {SearchAttribute::BirthYear, IndexBackend::Tree},
        {SearchAttribute::BirthYear, IndexBackend::SortedArray}
    });
    if (!built) return std::unexpected(built.error());
    return service;
}

template <typename Action>
Measurement measureMedian(Action action) {
    constexpr std::size_t repetitions = 5;
    benchmarkChecksumSink = action().checksum;
    ArraySortedSequence<double> samples;
    std::size_t matches = 0;
    for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
        const auto start = std::chrono::steady_clock::now();
        const BatchResult batch = action();
        const auto finish = std::chrono::steady_clock::now();
        benchmarkChecksumSink = batch.checksum;
        matches = batch.matchCount;
        const auto elapsed = std::chrono::duration<double, std::micro>(finish - start).count();
        samples.add(elapsed);
    }
    return {samples.at(repetitions / 2), matches};
}

template <typename Structure, typename Builder>
std::expected<Measurement, Error> measureBuild(Builder builder) {
    constexpr std::size_t repetitions = 5;
    {
        Structure warmup;
        const auto built = builder(warmup);
        if (!built) return std::unexpected(built.error());
        benchmarkChecksumSink = *built;
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
        benchmarkChecksumSink = count;
        samples.add(std::chrono::duration<double, std::micro>(finish - start).count());
    }
    return Measurement{samples.at(repetitions / 2), count};
}

bool sameIds(const std::expected<DynamicArray<RecordId>, Error>& left,
             const std::expected<DynamicArray<RecordId>, Error>& right) {
    if (!left || !right || left->size() != right->size()) return false;
    for (std::size_t index = 0; index < left->size(); ++index) {
        if ((*left)[index] != (*right)[index]) return false;
    }
    return true;
}

bool verifyExact(const StudentSearchService& service, const DynamicArray<int>& queries) {
    for (const int query : queries) {
        const auto reference = service.findByBirthYear(query, ExactSearchStrategy::Linear);
        if (!sameIds(reference, service.findByBirthYear(query, ExactSearchStrategy::Tree)) ||
            !sameIds(reference, service.findByBirthYear(query, ExactSearchStrategy::Binary))) return false;
    }
    return true;
}

bool verifyRanges(
    const StudentSearchService& service,
    const DynamicArray<TwoElement<int, int>>& queries
) {
    for (const auto& query : queries) {
        if (!sameIds(service.findInBirthYearRange(query.first, query.second, RangeSearchStrategy::Linear),
                     service.findInBirthYearRange(query.first, query.second, RangeSearchStrategy::Binary))) return false;
    }
    return true;
}

void accumulateSearchResult(
    BatchResult& batch,
    const std::expected<DynamicArray<RecordId>, Error>& ids
) {
    if (!ids) {
        batch.checksum ^= 0xffffffffffffffffULL;
        return;
    }
    batch.matchCount += ids->size();
    batch.checksum = (batch.checksum ^ ids->size()) * 1099511628211ULL;
    for (const RecordId id : *ids) batch.checksum = (batch.checksum ^ id) * 1099511628211ULL;
}

BatchResult runExactSearchBatch(
    const StudentSearchService& service,
    const DynamicArray<int>& queries,
    ExactSearchStrategy strategy
) {
    BatchResult batch;
    for (const int query : queries) accumulateSearchResult(batch, service.findByBirthYear(query, strategy));
    return batch;
}

BatchResult runRangeSearchBatch(
    const StudentSearchService& service,
    const DynamicArray<TwoElement<int, int>>& queries,
    RangeSearchStrategy strategy
) {
    BatchResult batch;
    for (const auto& query : queries) {
        accumulateSearchResult(batch, service.findInBirthYearRange(query.first, query.second, strategy));
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
std::expected<DynamicArray<BenchmarkResult>, Error> BenchmarkRunner::run(
    std::size_t datasetSize,
    std::size_t queryCount,
    std::uint64_t seed
) {
    if (datasetSize > maxDatasetSize || queryCount > maxQueryCount ||
        (datasetSize != 0 && queryCount > maxWorkload / datasetSize)) {
        return std::unexpected(makeError(ErrorCode::InvalidWorkload));
    }
    DynamicArray<BenchmarkResult> results;
    for (int orderNumber = 0; orderNumber < 3; ++orderNumber) {
        const auto order = static_cast<InputOrder>(orderNumber);
        const DynamicArray<int> values = makeInputKeys(datasetSize, order, seed);
        const DynamicArray<int> queries = makeExactQueries(datasetSize, queryCount, seed);
        const auto rangeQueries = makeRangeQueries(datasetSize, queryCount);
        const std::string name = orderName(order);

        const auto treeBuild = measureBuild<TreeDictionary<int, std::size_t>>(
            [&values](auto& tree) -> std::expected<std::size_t, Error> {
                for (std::size_t index = 0; index < values.size(); ++index) {
                    const auto inserted = tree.add(values[index], index);
                    if (!inserted) {
                        return std::unexpected(makeError(ErrorCode::BenchmarkPreparation,
                            {.detail = "cannot build benchmark tree: duplicate key"}));
                    }
                }
                return tree.getCount();
            });
        if (!treeBuild) return std::unexpected(treeBuild.error());
        appendResult(results, datasetSize, name, "TreeDictionary", "Build", 1,
                     *treeBuild, seed, treeBuild->matchCount == datasetSize);

        const auto sortedBuild = measureBuild<SortedArrayDictionary<int, std::size_t>>(
            [&values](auto& dictionary) -> std::expected<std::size_t, Error> {
                for (std::size_t index = 0; index < values.size(); ++index) {
                    if (const auto added = dictionary.add(values[index], index); !added) {
                        return std::unexpected(added.error());
                    }
                }
                return dictionary.getCount();
            });
        if (!sortedBuild) return std::unexpected(sortedBuild.error());
        appendResult(results, datasetSize, name, "SortedArrayDictionary", "Build", 1,
                     *sortedBuild, seed, sortedBuild->matchCount == datasetSize);

        const auto prepared = makeBenchmarkService(values);
        if (!prepared) {
            return std::unexpected(makeError(ErrorCode::BenchmarkPreparation,
                {.detail = "cannot prepare benchmark data: " + formatError(prepared.error())}));
        }
        const StudentSearchService& service = *prepared;
        const bool exactVerified = verifyExact(service, queries);
        appendResult(results, datasetSize, name, "Linear", "ExactSearch", queryCount,
                     measureMedian([&]() { return runExactSearchBatch(service, queries, ExactSearchStrategy::Linear); }),
                     seed, exactVerified);
        appendResult(results, datasetSize, name, "TreeDictionary", "ExactSearch", queryCount,
                     measureMedian([&]() { return runExactSearchBatch(service, queries, ExactSearchStrategy::Tree); }),
                     seed, exactVerified);
        appendResult(results, datasetSize, name, "BinarySearch", "ExactSearch", queryCount,
                     measureMedian([&]() { return runExactSearchBatch(service, queries, ExactSearchStrategy::Binary); }),
                     seed, exactVerified);

        const bool rangeVerified = verifyRanges(service, rangeQueries);
        appendResult(results, datasetSize, name, "Linear", "RangeSearch", queryCount,
                     measureMedian([&]() { return runRangeSearchBatch(service, rangeQueries, RangeSearchStrategy::Linear); }),
                     seed, rangeVerified);
        appendResult(results, datasetSize, name, "BinarySearch", "RangeSearch", queryCount,
                     measureMedian([&]() { return runRangeSearchBatch(service, rangeQueries, RangeSearchStrategy::Binary); }),
                     seed, rangeVerified);
    }
    return results;
}

std::expected<void, Error> BenchmarkRunner::saveCsv(
    const std::filesystem::path& path,
    const DynamicArray<BenchmarkResult>& results
) {
    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";
    std::ofstream output(temporaryPath, std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output.is_open()) {
        return std::unexpected(makeError(ErrorCode::FileOpen,
            {.path = path, .detail = "cannot open benchmark CSV"}));
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
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeError(ErrorCode::FileWrite,
            {.path = path, .detail = "failed to write benchmark CSV"}));
    }
    output.close();
    std::error_code renameError;
    std::filesystem::rename(temporaryPath, path, renameError);
    if (renameError) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeError(ErrorCode::FileWrite,
            {.path = path, .detail = renameError.message()}));
    }
    return {};
}

std::expected<DynamicArray<BenchmarkResult>, Error> BenchmarkRunner::runAndSave(
    const std::filesystem::path& path,
    std::size_t datasetSize,
    std::size_t queryCount,
    std::uint64_t seed
) {
    auto results = run(datasetSize, queryCount, seed);
    if (!results) return std::unexpected(results.error());
    if (auto saved = saveCsv(path, *results); !saved) {
        return std::unexpected(saved.error());
    }
    return results;
}
