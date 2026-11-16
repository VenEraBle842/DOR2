#include "benchmark/benchmark_runner.hpp"
#include "service/student_search_service.hpp"
#include "storage/student_file_storage.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <locale>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

constexpr std::size_t maxGeneratedRecords = 10'000;

template <typename T>
std::optional<T> readNumber(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    std::istringstream input(line);
    input.imbue(std::locale::classic());
    input >> std::ws;
    if constexpr (std::is_unsigned_v<T>) {
        if (input.peek() == '-') {
            std::cout << "Value must not be negative\n";
            return std::nullopt;
        }
    }
    T value{};
    if (!(input >> value)) {
        std::cout << "Invalid value.\n";
        return std::nullopt;
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        std::cout << "Unexpected characters after value\n";
        return std::nullopt;
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
            std::cout << "Value must be finite\n";
            return std::nullopt;
        }
    }
    return value;
}

std::optional<std::size_t> readSize(const std::string& prompt, std::size_t maximum) {
    const auto value = readNumber<std::size_t>(prompt);
    if (value && *value > maximum) {
        std::cout << "Value must not exceed " << maximum << ".\n";
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> readText(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    if (!std::getline(std::cin, value)) return std::nullopt;
    return value;
}

std::string serviceErrorMessage(ServiceError error) {
    switch (error) {
        case ServiceError::IndexNotBuilt: return "indexes are not built";
        case ServiceError::IndexStale: return "indexes are stale; rebuild them";
        case ServiceError::InvalidRange: return "range upper bound is below lower bound";
        case ServiceError::InvalidKey: return "GPA must be finite";
        case ServiceError::VerificationFailed: return "search result verification failed";
    }
    return "unknown search error";
}

std::optional<SearchStrategy> readSearchStrategy() {
    std::cout << "Algorithm: 1 - linear, 2 - tree";
    std::cout << ", 3 - binary";
    std::cout << "\n";
    const auto choice = readNumber<int>("> ");
    if (!choice) return std::nullopt;
    if (*choice == 1) return SearchStrategy::Linear;
    if (*choice == 2) return SearchStrategy::Tree;
    if (*choice == 3) return SearchStrategy::Binary;
    std::cout << "Unknown algorithm\n";
    return std::nullopt;
}

std::optional<RangeStrategy> readRangeStrategy() {
    const auto choice = readNumber<int>("Algorithm: 1 - linear, 2 - binary\n> ");
    if (!choice) return std::nullopt;
    if (*choice == 1) return RangeStrategy::Linear;
    if (*choice == 2) return RangeStrategy::Binary;
    std::cout << "Unknown algorithm\n";
    return std::nullopt;
}

void printStudent(const Student& student) {
    std::cout << "ID=" << student.id << ", " << student.firstName << ' ' << student.lastName
              << ", birth year=" << student.birthYear << ", GPA=" << student.gpa << '\n';
}

void printResults(
    const StudentSearchService& service,
    const std::expected<DynamicArray<RecordId>, ServiceError>& result
) {
    if (!result) {
        std::cout << "Search failed: " << serviceErrorMessage(result.error()) << "\n";
        return;
    }
    std::cout << "Matches: " << result->size() << '\n';
    for (const RecordId id : *result) {
        const auto student = service.getRepository().findById(id);
        if (student) printStudent(*student);
    }
}

void addStudent(StudentSearchService& service) {
    const auto id = readNumber<std::uint64_t>("ID: ");
    const auto firstName = readText("First name: ");
    const auto lastName = readText("Last name: ");
    const auto birthYear = readNumber<int>("Birth year: ");
    const auto gpa = readNumber<double>("GPA: ");
    if (!id || !firstName || !lastName || !birthYear || !gpa) return;
    if (!std::isfinite(*gpa)) {
        std::cout << "GPA must be finite\n";
        return;
    }
    const auto added = service.addStudent(Student{*id, *firstName, *lastName, *birthYear, *gpa});
    if (!added) {
        std::cout << "Add failed: " << toString(added.error()) << '\n';
        return;
    }
    std::cout << "Student added; indexes are now stale\n";
}

void generateStudents(StudentSearchService& service) {
    const auto count = readSize("Record count (0-10000): ", maxGeneratedRecords);
    if (!count) return;
    const auto seed = readNumber<std::uint64_t>("Seed: ");
    if (!count || !seed) return;
    DynamicArray<Student> students;
    std::mt19937_64 generator(*seed);
    for (std::size_t index = 0; index < *count; ++index) {
        const int year = 1998 + static_cast<int>(generator() % 10U);
        const double gpa = 3.0 + static_cast<double>(generator() % 21U) / 10.0;
        students.push_back(Student{
            1000U + index,
            "First" + std::to_string(generator() % 23U),
            "Last" + std::to_string(generator() % 17U),
            year,
            gpa
        });
    }
    const auto replaced = service.replaceStudents(std::move(students));
    if (!replaced) {
        std::cout << "Generation failed: " << toString(replaced.error()) << '\n';
        return;
    }
    std::cout << "Generated " << *count << " records. Build indexes before indexed search\n";
}

void showStudents(const StudentSearchService& service) {
    const auto& repository = service.getRepository();
    std::cout << "Records: " << repository.getCount() << '\n';
    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        printStudent(repository.at(position));
    }
}

void loadStudents(StudentSearchService& service) {
    const auto path = readText("Input file: ");
    if (!path) return;
    auto loaded = StudentFileStorage::load(*path);
    if (!loaded) {
        std::cout << "Load failed (" << toString(loaded.error().code) << ")";
        if (loaded.error().line != 0) std::cout << " at line " << loaded.error().line;
        std::cout << ": " << loaded.error().message << '\n';
        return;
    }
    const auto replaced = service.replaceStudents(std::move(loaded.value()));
    if (!replaced) {
        std::cout << "Load failed: " << toString(replaced.error()) << '\n';
        return;
    }
    service.buildIndexes();
    std::cout << "Loaded " << service.getRepository().getCount() << " records and rebuilt indexes\n";
}

void saveStudents(const StudentSearchService& service) {
    const auto path = readText("Output file: ");
    if (!path) return;
    const auto saved = StudentFileStorage::save(*path, service.getRepository());
    if (!saved) {
        std::cout << "Save failed: " << saved.error().message << '\n';
        return;
    }
    std::cout << "Data saved.\n";
}

void showStatistics(const StudentSearchService& service) {
    const auto statistics = service.getIndexStatistics();
    if (!statistics) {
        std::cout << "Cannot show statistics: " << toString(statistics.error()) << "\n";
        return;
    }
    std::cout << "Records=" << statistics->records
              << ", last-name keys=" << statistics->lastNameKeys
              << ", birth-year keys=" << statistics->birthYearKeys
              << ", GPA keys=" << statistics->gpaKeys
              << ", composite keys=" << statistics->compositeKeys << '\n';
}

void exactSearch(const StudentSearchService& service) {
    const auto attribute = readNumber<int>(
        "Attribute: 1 - ID, 2 - last name, 3 - birth year, 4 - GPA\n> "
    );
    if (!attribute) return;
    const auto strategy = readSearchStrategy();
    if (!strategy) return;
    if (*attribute == 1) {
        const auto value = readNumber<RecordId>("ID: ");
        if (value) printResults(service, service.findById(*value, *strategy));
    } else if (*attribute == 2) {
        const auto value = readText("Last name: ");
        if (value) printResults(service, service.findByLastName(*value, *strategy));
    } else if (*attribute == 3) {
        const auto value = readNumber<int>("Birth year: ");
        if (value) printResults(service, service.findByBirthYear(*value, *strategy));
    } else if (*attribute == 4) {
        const auto value = readNumber<double>("GPA: ");
        if (value) printResults(service, service.findByGpa(*value, *strategy));
    } else {
        std::cout << "Unknown attribute\n";
    }
}

void rangeSearchMenu(const StudentSearchService& service) {
    const auto attribute = readNumber<int>("Attribute: 1 - birth year, 2 - GPA\n> ");
    if (!attribute) return;
    const auto strategy = readRangeStrategy();
    if (!strategy) return;
    if (*attribute == 1) {
        const auto lower = readNumber<int>("Lower year: ");
        const auto upper = readNumber<int>("Upper year: ");
        if (lower && upper) printResults(service, service.findBirthYearRange(*lower, *upper, *strategy));
    } else if (*attribute == 2) {
        const auto lower = readNumber<double>("Lower GPA: ");
        const auto upper = readNumber<double>("Upper GPA: ");
        if (lower && upper) printResults(service, service.findGpaRange(*lower, *upper, *strategy));
    } else {
        std::cout << "Unknown attribute\n";
    }
}

std::optional<StudentCompositeKey> readCompositeKey() {
    const auto lastName = readText("Last name: ");
    if (!lastName) return std::nullopt;
    const auto firstName = readText("First name: ");
    if (!firstName) return std::nullopt;
    const auto birthYear = readNumber<int>("Birth year: ");
    if (!birthYear) return std::nullopt;
    return StudentCompositeKey{*lastName, *firstName, *birthYear};
}

void compositeSearch(const StudentSearchService& service) {
    const auto mode = readNumber<int>("Search: 1 - exact, 2 - range\n> ");
    if (!mode) return;
    if (*mode == 1) {
        const auto key = readCompositeKey();
        if (!key) return;
        const auto strategy = readSearchStrategy();
        if (strategy) printResults(service, service.findByComposite(*key, *strategy));
    } else if (*mode == 2) {
        std::cout << "Inclusive lexicographic range: last name, first name, birth year\n"
                  << "Lower key:\n";
        const auto lower = readCompositeKey();
        if (!lower) return;
        std::cout << "Upper key:\n";
        const auto upper = readCompositeKey();
        if (!upper) return;
        const auto strategy = readRangeStrategy();
        if (strategy) printResults(service, service.findCompositeRange(*lower, *upper, *strategy));
    } else {
        std::cout << "Unknown search mode\n";
    }
}

void runBenchmark(DynamicArray<BenchmarkResult>& latestResults) {
    const auto size = readSize("Dataset size (0-10000): ", BenchmarkRunner::maxDatasetSize);
    if (!size) return;
    const auto queries = readSize("Query count (0-10000): ", BenchmarkRunner::maxQueryCount);
    if (!queries) return;
    const auto seed = readNumber<std::uint64_t>("Seed: ");
    if (!size || !queries || !seed) return;
    auto results = BenchmarkRunner::run(*size, *queries, *seed);
    if (!results) {
        std::cout << "Benchmark failed: " << results.error().message << '\n';
        return;
    }
    latestResults = std::move(*results);
    std::cout << "Benchmark rows: " << latestResults.size() << '\n';
    for (const BenchmarkResult& result : latestResults) {
        std::cout << result.inputOrder << " | " << result.algorithm << " | " << result.operation
                  << " | " << result.elapsedUs << " us | matches=" << result.matchCount
                  << " | " << (result.verified ? "verified" : "FAILED") << '\n';
    }
}

void exportBenchmark(const DynamicArray<BenchmarkResult>& latestResults) {
    if (latestResults.empty()) {
        std::cout << "Run a comparison first.\n";
        return;
    }
    const auto path = readText("CSV output file: ");
    if (!path) return;
    const auto saved = BenchmarkRunner::saveCsv(*path, latestResults);
    if (saved) {
        std::cout << "CSV saved\n";
    } else {
        std::cout << "CSV save failed: " << saved.error().message << '\n';
    }
}

void printMenu() {
    std::cout << "\n=== Student Indexing ===\n"
              << " 1. Add student manually\n"
              << " 2. Generate data automatically\n"
              << " 3. View source records\n"
              << " 4. Load records from file\n"
              << " 5. Save records to file\n"
              << " 6. Build/rebuild indexes\n"
              << " 7. Show index statistics\n"
              << " 8. Exact search\n"
              << " 9. Range search\n"
              << "10. Composite-key search\n"
              << "11. Compare algorithms\n"
              << "12. Export latest comparison to CSV\n"
              << " 0. Exit\n";
}
int main() {
    StudentSearchService service;
    DynamicArray<BenchmarkResult> latestResults;
    while (std::cin) {
        printMenu();
        const auto choice = readNumber<int>("> ");
        if (!choice) {
            if (std::cin.eof()) break;
            continue;
        }
        if (*choice == 0) break;
        switch (*choice) {
            case 1: addStudent(service); break;
            case 2: generateStudents(service); break;
            case 3: showStudents(service); break;
            case 4: loadStudents(service); break;
            case 5: saveStudents(service); break;
            case 6:
                service.buildIndexes();
                std::cout << "Indexes built.\n";
                break;
            case 7: showStatistics(service); break;
            case 8: exactSearch(service); break;
            case 9: rangeSearchMenu(service); break;
            case 10: compositeSearch(service); break;
            case 11: runBenchmark(latestResults); break;
            case 12: exportBenchmark(latestResults); break;
            default: std::cout << "Unknown menu option\n"; break;
        }
    }
    std::cout << "Program finished\n";
    return 0;
}
