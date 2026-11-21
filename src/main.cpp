#include "application/student_application.hpp"
#include "common/number_input.hpp"
#include "service/student_search_service.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <sstream>
#include <optional>
#include <string>

void printError(const Error& error) {
    std::cout << formatError(error) << '\n';
}

template <typename T>
std::optional<T> readNumber(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    const auto value = parseNumber<T>(line, prompt);
    if (!value) { printError(value.error()); return std::nullopt; }
    return *value;
}

std::optional<std::size_t> readBoundedSize(const std::string& prompt, std::size_t maximum) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    const auto value = parseBoundedSize(line, prompt, maximum);
    if (!value) { printError(value.error()); return std::nullopt; }
    return *value;
}

std::optional<std::string> readText(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    if (!std::getline(std::cin, value)) return std::nullopt;
    return value;
}

std::optional<ExactSearchStrategy> readSearchStrategy() {
    std::cout << "Algorithm: 1 - linear, 2 - tree";
    std::cout << ", 3 - binary";
    std::cout << "\n";
    const auto choice = readNumber<int>("> ");
    if (!choice) return std::nullopt;
    if (*choice == 1) return ExactSearchStrategy::Linear;
    if (*choice == 2) return ExactSearchStrategy::Tree;
    if (*choice == 3) return ExactSearchStrategy::Binary;
    printError(makeError(ErrorCode::UnknownSelection, {.field = "Algorithm"}));
    return std::nullopt;
}

std::optional<RangeSearchStrategy> readRangeStrategy() {
    const auto choice = readNumber<int>("Algorithm: 1 - linear, 2 - binary\n> ");
    if (!choice) return std::nullopt;
    if (*choice == 1) return RangeSearchStrategy::Linear;
    if (*choice == 2) return RangeSearchStrategy::Binary;
    printError(makeError(ErrorCode::UnknownSelection, {.field = "Algorithm"}));
    return std::nullopt;
}

void printStudent(const Student& student) {
    std::cout << "ID=" << student.id << ", " << student.firstName << ' ' << student.lastName
              << ", birth year=" << student.birthYear << ", GPA=" << student.gpa << '\n';
}

void printSearchResults(
    const StudentSearchService& service,
    const std::expected<StudentRecordReferences, Error>& result
) {
    if (!result) {
        printError(result.error());
        return;
    }
    std::cout << "Matches: " << result->size() << '\n';
    for (const auto& reference : *result) {
        const auto student = service.resolve(reference);
        if (student) printStudent(*student);
        else printError(student.error());
    }
}

void addStudent(StudentApplication& app) {
    const auto id = readNumber<std::uint64_t>("ID: ");
    const auto firstName = readText("First name: ");
    const auto lastName = readText("Last name: ");
    const auto birthYear = readNumber<int>("Birth year: ");
    const auto gpa = readNumber<double>("GPA: ");
    if (!id || !firstName || !lastName || !birthYear || !gpa) return;
    const auto added = app.addStudent(Student{*id, *firstName, *lastName, *birthYear, *gpa});
    if (!added) {
        printError(added.error());
        return;
    }
    std::cout << "Student added; indexes are now stale\n";
}

void generateStudents(StudentApplication& app) {
    const auto count = readBoundedSize("Record count (0-10000): ", StudentApplication::maxGeneratedRecords);
    if (!count) return;
    const auto seed = readNumber<std::uint64_t>("Seed: ");
    if (!seed) return;
    const auto result = app.generateStudents(*count, *seed);
    if (!result) { printError(result.error()); return; }
    std::cout << "Generated " << *count << " records. Build indexes before indexed search\n";
}

void showStudents(const StudentSearchService& service) {
    const auto& repository = service.getRepository();
    std::cout << "Records: " << repository.getCount() << '\n';
    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        printStudent(repository.at(position));
    }
}

void loadStudents(StudentApplication& app) {
    const auto path = readText("Input file: ");
    if (!path) return;
    const auto loaded = app.loadStudents(*path);
    if (!loaded) { printError(loaded.error()); return; }
    std::cout << "Loaded " << app.getSearchService().getRepository().getCount()
              << " records and rebuilt indexes\n";
}

void saveStudents(const StudentApplication& app) {
    const auto path = readText("Output file: ");
    if (!path) return;
    const auto saved = app.saveStudents(*path);
    if (!saved) { printError(saved.error()); return; }
    std::cout << "Data saved.\n";
}

void showIndexStatistics(const StudentSearchService& service) {
    const auto statistics = service.getIndexStatistics();
    std::cout << "Records=" << statistics.recordCount << '\n';
    for (const auto& state : statistics.indexes) {
        std::cout << state.name << (state.selection.backend == IndexBackend::Tree ? " / Tree: " : " / Sorted array: ")
                  << (!state.built ? "not built" : state.stale ? "out of date" : "ready");
        if (state.built) std::cout << ", keys=" << state.keyCount;
        std::cout << '\n';
    }
}

void buildSelectedIndexes(StudentApplication& app) {
    const auto& service = app.getSearchService();
    const auto states = service.getIndexes().getStates(service.getRepository());
    std::cout << "0. All indexes\n";
    for (std::size_t index = 0; index < states.size(); ++index) {
        std::cout << index + 1 << ". " << states[index].name
                  << (states[index].selection.backend == IndexBackend::Tree
                      ? " / Tree\n" : " / Sorted array\n");
    }
    const auto input = readText("Index numbers separated by spaces (0 = all): ");
    if (!input) return;
    std::istringstream numbers(*input);
    DynamicArray<IndexSelection> selections;
    std::string token;
    while (numbers >> token) {
        const auto number = parseBoundedSize(token, "Index", states.size());
        if (!number) {
            printError(number.error());
            return;
        }
        if (*number == 0) {
            const bool hasMoreNumbers = static_cast<bool>(numbers >> token);
            if (!selections.empty() || hasMoreNumbers) {
                printError(makeError(ErrorCode::UnknownSelection,
                    {.field = "Index", .detail = "Use 0 alone to build all indexes."}));
                return;
            }
            for (const auto& state : states) {
                selections.push_back(state.selection);
            }
            break;
        }
        selections.push_back(states[*number - 1].selection);
    }
    if (selections.empty()) {
        printError(makeError(ErrorCode::UnknownSelection, {.field = "Index"}));
        return;
    }
    if (const auto built = app.buildIndexes(selections); !built) {
        printError(built.error());
        return;
    }
    std::cout << "Selected indexes built.\n";
}

std::optional<SearchAttributeDescription> readSearchAttribute(bool range, bool multipleFieldsOnly) {
    const auto attributes = defaultSearchHandlers().getAttributes(range);
    DynamicArray<SearchAttributeDescription> choices;
    for (const auto& attribute : attributes) {
        if (!multipleFieldsOnly || attribute.fieldNames.size() > 1) {
            choices.push_back(attribute);
        }
    }
    if (choices.empty()) {
        printError(makeError(ErrorCode::UnknownSelection, {.field = "Search attribute"}));
        return std::nullopt;
    }
    if (multipleFieldsOnly && choices.size() == 1) return choices[0];

    std::cout << "Attribute:\n";
    for (std::size_t index = 0; index < choices.size(); ++index) {
        std::cout << index + 1 << ". " << choices[index].name << '\n';
    }
    const auto choice = readBoundedSize("> ", choices.size());
    if (!choice) return std::nullopt;
    if (*choice == 0) {
        printError(makeError(ErrorCode::UnknownSelection, {.field = "Search attribute"}));
        return std::nullopt;
    }
    return choices[*choice - 1];
}

std::optional<SearchKeyInput> readSearchKey(const SearchAttributeDescription& attribute) {
    SearchKeyInput result;
    for (const auto& field : attribute.fieldNames) {
        const auto value = readText(field + ": ");
        if (!value) return std::nullopt;
        result.push_back(*value);
    }
    return result;
}

void handleSearch(const StudentApplication& app, bool range, bool multipleFieldsOnly = false) {
    const auto attribute = readSearchAttribute(range, multipleFieldsOnly);
    if (!attribute) return;
    SearchRequest request;
    request.attribute = attribute->id;
    request.range = range;
    if (range) {
        const auto strategy = readRangeStrategy();
        if (!strategy) return;
        request.rangeStrategy = *strategy;
    } else {
        const auto strategy = readSearchStrategy();
        if (!strategy) return;
        request.exactStrategy = *strategy;
    }
    if (!attribute->hint.empty()) std::cout << attribute->hint << '\n';
    if (range) std::cout << "Lower key (inclusive):\n";
    const auto lower = readSearchKey(*attribute);
    if (!lower) return;
    request.lower = *lower;
    if (range) {
        std::cout << "Upper key (inclusive):\n";
        const auto upper = readSearchKey(*attribute);
        if (!upper) return;
        request.upper = *upper;
    }
    printSearchResults(app.getSearchService(), app.searchRecords(request));
}

void handleCompositeSearch(const StudentApplication& app) {
    const auto mode = readNumber<int>("Search: 1 - exact, 2 - range\n> ");
    if (!mode) return;
    if (*mode != 1 && *mode != 2) {
        printError(makeError(ErrorCode::UnknownSelection, {.field = "Search mode"}));
        return;
    }
    handleSearch(app, *mode == 2, true);
}

void runBenchmark(StudentApplication& app) {
    const auto size = readBoundedSize("Dataset size (0-10000): ", BenchmarkRunner::maxDatasetSize);
    if (!size) return;
    const auto queries = readBoundedSize("Query count (0-10000): ", BenchmarkRunner::maxQueryCount);
    if (!queries) return;
    const auto seed = readNumber<std::uint64_t>("Seed: ");
    if (!size || !queries || !seed) return;
    auto results = app.runBenchmark(*size, *queries, *seed);
    if (!results) {
        printError(results.error());
        return;
    }
    const auto& latestResults = app.getLatestResults();
    std::cout << "Benchmark rows: " << latestResults.size() << '\n';
    for (const BenchmarkResult& result : latestResults) {
        std::cout << result.inputOrder << " | " << result.algorithm << " | " << result.operation
                  << " | " << result.elapsedUs << " us | matches=" << result.matchCount
                  << " | " << (result.verified ? "verified" : "FAILED") << '\n';
    }
}

void exportBenchmark(const StudentApplication& app) {
    if (app.getLatestResults().empty()) {
        printError(makeError(ErrorCode::NoBenchmarkResults));
        return;
    }
    const auto path = readText("CSV output file: ");
    if (!path) return;
    const auto saved = app.exportBenchmark(*path);
    if (!saved) { printError(saved.error()); return; }
    std::cout << "CSV saved\n";
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
              << "10. Multi-field key search\n"
              << "11. Compare algorithms\n"
              << "12. Export latest comparison to CSV\n"
              << " 0. Exit\n";
}
int main() {
    StudentApplication app;
    const auto& service = app.getSearchService();
    while (std::cin) {
        printMenu();
        const auto choice = readNumber<int>("> ");
        if (!choice) {
            if (std::cin.eof()) break;
            continue;
        }
        if (*choice == 0) break;
        try {
            switch (*choice) {
                case 1: addStudent(app); break;
                case 2: generateStudents(app); break;
                case 3: showStudents(service); break;
                case 4: loadStudents(app); break;
                case 5: saveStudents(app); break;
                case 6: buildSelectedIndexes(app); break;
                case 7: showIndexStatistics(service); break;
                case 8: handleSearch(app, false); break;
                case 9: handleSearch(app, true); break;
                case 10: handleCompositeSearch(app); break;
                case 11: runBenchmark(app); break;
                case 12: exportBenchmark(app); break;
                default: printError(makeError(ErrorCode::UnknownSelection, {.field = "Menu"})); break;
            }
        } catch (const std::exception& error) {
            printError(makeError(ErrorCode::Unexpected, {.detail = error.what()}));
        } catch (...) {
            printError(makeError(ErrorCode::Unexpected));
        }
    }
    std::cout << "Program finished\n";
    return 0;
}
