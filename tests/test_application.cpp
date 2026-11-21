#include <gtest/gtest.h>

#include "application/student_application.hpp"
#include "common/number_input.hpp"
#include "common/two_element.hpp"
#include "storage/student_file_storage.hpp"

#include <chrono>
#include <fstream>
#include <limits>

class ApplicationTest : public testing::Test {
protected:
    StudentApplication app_;
    std::filesystem::path directory_ = std::filesystem::temp_directory_path() /
        ("dor2-app-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    void SetUp() override { std::filesystem::create_directory(directory_); }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    void populate() {
        ASSERT_TRUE(app_.addStudent({8, "Anna", "Smith", 2003, 4.5}));
        ASSERT_TRUE(app_.addStudent({2, "Anna", "Smith", 2003, 4.5}));
        ASSERT_TRUE(app_.addStudent({5, "Ivan", "Smith", 2001, 4.8}));
        ASSERT_TRUE(app_.addStudent({1, "Иван", "Иванов", 2004, 4.1}));
    }
};

TEST(NumberInputTest, RejectsInvalidAndOutOfRangeValuesWithoutThrowing) {
    EXPECT_EQ(parseNumber<std::uint64_t>("-1", "ID").error().code, ErrorCode::NegativeNumber);
    EXPECT_FALSE(parseNumber<std::uint64_t>("18446744073709551616", "ID"));
    EXPECT_FALSE(parseNumber<int>("2003x", "Year"));
    EXPECT_FALSE(parseNumber<double>("4,5", "GPA"));
    EXPECT_FALSE(parseNumber<double>("nan", "GPA"));
    EXPECT_FALSE(parseNumber<double>("inf", "GPA"));
    EXPECT_FALSE(parseNumber<int>("", "Year"));
    EXPECT_EQ(parseBoundedSize("10001", "Count", 10000).error().code, ErrorCode::LimitExceeded);
    EXPECT_EQ(*parseNumber<std::uint64_t>("18446744073709551615", "ID"), std::numeric_limits<std::uint64_t>::max());
    EXPECT_DOUBLE_EQ(*parseNumber<double>(" 4.125 ", "GPA"), 4.125);
}

TEST(ErrorCatalogTest, EveryCodeHasANameMessageAndRecoveryHint) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(ErrorCode::Count); ++i) {
        const auto& entry = describeError(static_cast<ErrorCode>(i));
        EXPECT_FALSE(entry.name.empty());
        EXPECT_FALSE(entry.message.empty());
        EXPECT_FALSE(entry.hint.empty());
    }
    const Error failure = makeError(ErrorCode::FileRecord,
        {.path = "students.txt", .lineNumber = 7, .detail = "broken GPA"});
    const auto formatted = formatError(failure);
    EXPECT_NE(formatted.find("students.txt"), std::string::npos);
    EXPECT_NE(formatted.find("Line: 7"), std::string::npos);
    EXPECT_NE(formatted.find("broken GPA"), std::string::npos);
    EXPECT_EQ(describeError(ErrorCode::Count).name, "Unexpected");
}

TEST_F(ApplicationTest, FileErrorContextIsPreservedAcrossTheApplicationBoundary) {
    const auto path = directory_ / "missing.txt";
    const auto stored = StudentFileStorage::load(path);
    const auto loaded = app_.loadStudents(path);
    ASSERT_FALSE(stored);
    ASSERT_FALSE(loaded);
    EXPECT_EQ(loaded.error().code, stored.error().code);
    EXPECT_EQ(loaded.error().context.path, path);
    EXPECT_EQ(loaded.error().context.lineNumber, stored.error().context.lineNumber);
    EXPECT_EQ(loaded.error().context.detail, stored.error().context.detail);
}

TEST_F(ApplicationTest, BenchmarkSaveErrorKeepsItsOriginalCodeAndFileContext) {
    const auto path = directory_ / "missing" / "comparison.csv";
    const auto result = BenchmarkRunner::runAndSave(path, 5, 2, 100);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::FileOpen);
    EXPECT_EQ(result.error().context.path, path);
    EXPECT_EQ(result.error().context.detail, "cannot open benchmark CSV");
}

TEST_F(ApplicationTest, DuplicateAndInvalidGpaUseCommonErrorsWithoutChangingRecords) {
    populate();
    EXPECT_EQ(app_.addStudent({8, "Other", "Name", 2000, 3.0}).error().code, ErrorCode::DuplicateId);
    EXPECT_EQ(app_.addStudent({9, "Other", "Name", 2000, std::numeric_limits<double>::infinity()}).error().code,
              ErrorCode::NonFiniteNumber);
    EXPECT_EQ(app_.getSearchService().getRepository().getCount(), 4U);
}

TEST_F(ApplicationTest, GenerationIsReproducibleBoundedAndSupportsEmptyData) {
    ASSERT_TRUE(app_.generateStudents(25, 123));
    StudentApplication second;
    ASSERT_TRUE(second.generateStudents(25, 123));
    for (std::size_t i = 0; i < 25; ++i) {
        EXPECT_EQ(app_.getSearchService().getRepository().at(i), second.getSearchService().getRepository().at(i));
    }
    EXPECT_EQ(app_.generateStudents(10001, 1).error().code, ErrorCode::LimitExceeded);
    EXPECT_EQ(app_.getSearchService().getRepository().getCount(), 25U);
    ASSERT_TRUE(app_.generateStudents(0, 123));
    EXPECT_TRUE(app_.getSearchService().getRepository().isEmpty());
}

TEST_F(ApplicationTest, ExactRequestsAgreeForEveryAttributeAndStrategy) {
    populate();
    ASSERT_TRUE(app_.buildIndexes());
    const SearchKeyInput keys[] = {{"8"}, {"Smith"}, {"2003"},
                                    {"4.5"}, {"Smith", "Anna", "2003"}};
    const std::size_t counts[] = {1, 3, 2, 2, 2};
    for (int attribute = 0; attribute < 5; ++attribute) {
        SearchRequest request;
        request.attribute = static_cast<SearchAttribute>(attribute);
        request.lower = keys[attribute];
        const auto reference = app_.search(request);
        ASSERT_TRUE(reference);
        ASSERT_EQ(reference->size(), counts[attribute]);
        for (auto strategy : {ExactSearchStrategy::Tree, ExactSearchStrategy::Binary}) {
            request.exactStrategy = strategy;
            const auto actual = app_.search(request);
            ASSERT_TRUE(actual);
            ASSERT_EQ(actual->size(), reference->size());
            for (std::size_t i = 0; i < reference->size(); ++i) EXPECT_EQ((*actual)[i], (*reference)[i]);
        }
    }
}

TEST_F(ApplicationTest, RangeRequestsIncludeBoundsAndKeepKeyThenIdOrder) {
    populate();
    ASSERT_TRUE(app_.buildIndexes());
    SearchRequest request;
    request.range = true;
    request.attribute = SearchAttribute::CompositeKey;
    request.lower = {"Smith", "Anna", "2003"};
    request.upper = {"Smith", "Ivan", "2001"};
    for (const auto strategy : {RangeSearchStrategy::Linear, RangeSearchStrategy::Binary}) {
        request.rangeStrategy = strategy;
        const auto result = app_.search(request);
        ASSERT_TRUE(result);
        ASSERT_EQ(result->size(), 3U);
        EXPECT_EQ((*result)[0], 2U);
        EXPECT_EQ((*result)[1], 8U);
        EXPECT_EQ((*result)[2], 5U);
    }
    request.attribute = SearchAttribute::Gpa;
    request.lower = {"4.1"};
    request.upper = {"4.5"};
    ASSERT_EQ(app_.search(request)->size(), 3U);
    request.attribute = SearchAttribute::BirthYear;
    request.lower = {"2003"};
    request.upper = {"2004"};
    ASSERT_EQ(app_.search(request)->size(), 3U);
    request.upper = {"2001"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidRange);
}

TEST_F(ApplicationTest, IndexStatesInputErrorsAndEmptyMatchesRemainDistinct) {
    populate();
    SearchRequest request;
    request.lower = {"8"};
    request.exactStrategy = ExactSearchStrategy::Tree;
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::IndexNotBuilt);
    ASSERT_TRUE(app_.buildIndexes());
    ASSERT_TRUE(app_.addStudent({9, "New", "Name", 2000, 4.0}));
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::IndexStale);
    request.exactStrategy = ExactSearchStrategy::Linear;
    ASSERT_EQ(app_.search(request)->size(), 1U);
    request.lower = {"999"};
    ASSERT_TRUE(app_.search(request));
    EXPECT_TRUE(app_.search(request)->empty());
    request.lower = {"-1"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::NegativeNumber);
}

TEST_F(ApplicationTest, FailedLoadPreservesRecordsAndBuiltIndexesWithErrorContext) {
    populate();
    ASSERT_TRUE(app_.buildIndexes());
    const auto path = directory_ / "bad.txt";
    { std::ofstream file(path); file << "DOR2_STUDENT_DATA 1\n2\n7:\"A\":\"B\":2000:4\nbroken\n"; }
    const auto loaded = app_.loadStudents(path);
    ASSERT_FALSE(loaded);
    EXPECT_EQ(loaded.error().code, ErrorCode::FileRecord);
    EXPECT_EQ(loaded.error().context.path, path);
    EXPECT_EQ(loaded.error().context.lineNumber, 4U);
    EXPECT_EQ(app_.getSearchService().getRepository().getCount(), 4U);
    EXPECT_EQ(app_.getSearchService().findById(8, ExactSearchStrategy::Tree)->size(), 1U);
}

TEST_F(ApplicationTest, SuccessfulLoadRebuildsIndexesAndRoundTripsUnicode) {
    populate();
    const auto path = directory_ / "студенты.txt";
    ASSERT_TRUE(app_.saveStudents(path));
    ASSERT_TRUE(app_.generateStudents(2, 100));
    ASSERT_TRUE(app_.loadStudents(path));
    EXPECT_EQ(app_.getSearchService().getRepository().getCount(), 4U);
    const auto statistics = app_.getSearchService().getIndexStatistics();
    EXPECT_EQ(statistics.recordCount, 4U);
    EXPECT_FALSE(statistics.indexes.empty());
    for (const auto& state : statistics.indexes) {
        EXPECT_TRUE(state.built);
        EXPECT_FALSE(state.stale);
    }
    EXPECT_EQ(app_.getSearchService().getRepository().findById(1)->firstName, "Иван");
    EXPECT_EQ(app_.saveStudents(directory_ / "missing" / "file.txt").error().code, ErrorCode::FileOpen);
}

TEST_F(ApplicationTest, ComparisonUsesSeparateDataKeepsLastSuccessAndExportsCsv) {
    populate();
    const auto path = directory_ / "comparison.csv";
    EXPECT_EQ(app_.exportBenchmark(path).error().code, ErrorCode::NoBenchmarkResults);
    EXPECT_FALSE(std::filesystem::exists(path));
    ASSERT_TRUE(app_.runBenchmark(20, 10, 100));
    ASSERT_EQ(app_.getLatestResults().size(), 21U);
    for (const auto& row : app_.getLatestResults()) EXPECT_TRUE(row.verified);
    EXPECT_EQ(app_.getSearchService().getRepository().getCount(), 4U);
    EXPECT_EQ(app_.runBenchmark(10000, 10000, 100).error().code, ErrorCode::InvalidWorkload);
    EXPECT_EQ(app_.getLatestResults().size(), 21U);
    ASSERT_TRUE(app_.exportBenchmark(path));
    std::ifstream file(path);
    std::string line;
    ASSERT_TRUE(static_cast<bool>(std::getline(file, line)));
    EXPECT_EQ(line, "DatasetSize,InputOrder,Algorithm,Operation,QueryCount,ElapsedUs,MatchCount,Seed,Verified");
    std::size_t count = 0;
    while (std::getline(file, line)) ++count;
    EXPECT_EQ(count, 21U);
}

TEST_F(ApplicationTest, RegisteredHandlerAndExtractorExtendSearchWithoutChangingDispatcher) {
    const StudentAttribute<int> length{
        static_cast<SearchAttribute>(99), "First name length",
        [](const Student& student) { return static_cast<int>(student.firstName.size()); },
        {"Length"},
        [](const SearchKeyInput& input) { return parseNumber<int>(input.at(0), "Length"); },
        true
    };
    SearchRequestHandlers handlers;
    handlers.registerAttribute(length);
    StudentSearchService service;
    ASSERT_TRUE(service.addStudent({1, "Anna", "Smith", 2000, 4.0}));
    ASSERT_TRUE(service.addStudent({2, "Bob", "Smith", 2000, 4.0}));
    service.registerIndex(length, IndexBackend::SortedArray);
    ASSERT_TRUE(service.buildIndexes({{length.id, IndexBackend::SortedArray}}));
    SearchRequest request;
    request.attribute = length.id;
    request.lower = {"4"};
    for (auto strategy : {ExactSearchStrategy::Linear, ExactSearchStrategy::Binary}) {
        request.exactStrategy = strategy;
        const auto result = executeSearch(service, request, handlers);
        ASSERT_TRUE(result);
        ASSERT_EQ(result->size(), 1U);
        EXPECT_EQ((*result)[0].recordId, 1U);
    }
    request.range = true;
    request.lower = {"3"};
    request.upper = {"4"};
    request.rangeStrategy = RangeSearchStrategy::Binary;
    const auto result = executeSearch(service, request, handlers);
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 2U);
    EXPECT_EQ((*result)[0].recordId, 2U);
    EXPECT_EQ((*result)[1].recordId, 1U);
    request.attribute = static_cast<SearchAttribute>(98);
    EXPECT_EQ(executeSearch(service, request, handlers).error().code, ErrorCode::UnknownSelection);
}

TEST_F(ApplicationTest, LoadingReplacesPartialIndexesAndBuildsNewSnapshot) {
    populate();
    ASSERT_TRUE(app_.buildIndexes({{SearchAttribute::BirthYear, IndexBackend::Tree}}));
    const auto reference = app_.getSearchService().findExact(studentBirthYearAttribute, 2003, ExactSearchStrategy::Tree);
    ASSERT_TRUE(reference);
    const auto path = directory_ / "students.txt";
    ASSERT_TRUE(app_.saveStudents(path));
    ASSERT_TRUE(app_.loadStudents(path));
    EXPECT_EQ(app_.getSearchService().resolve((*reference)[0]).error().code, ErrorCode::IndexStale);
    for (const auto& state : app_.getSearchService().getIndexes().getStates(app_.getSearchService().getRepository())) {
        EXPECT_TRUE(state.built);
        EXPECT_FALSE(state.stale);
    }
}

TEST_F(ApplicationTest, IncorrectFieldCountsReturnExpectedErrors) {
    populate();
    SearchRequest request;
    request.attribute = SearchAttribute::Id;
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidSearchKey);
    request.lower = {"8", "extra"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidSearchKey);
    request.range = true;
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::UnknownSelection);

    request.attribute = SearchAttribute::BirthYear;
    request.lower = {"2000"};
    request.upper = {};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidSearchKey);
    request.upper = {"2004", "extra"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidSearchKey);
    request.upper = {"2004"};
    ASSERT_TRUE(app_.search(request));

    request.range = false;
    request.attribute = SearchAttribute::CompositeKey;
    request.lower = {"Smith", "Anna"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidSearchKey);
    request.lower = {"Smith", "Anna", "invalid year"};
    EXPECT_EQ(app_.search(request).error().code, ErrorCode::InvalidNumber);
}

namespace {

using FourFieldKey = TwoElement<StudentCompositeKey, double>;

struct FourFieldCompare {
    bool operator()(const FourFieldKey& left, const FourFieldKey& right) const {
        const StudentCompositeKeyCompare compare;
        if (compare(left.first, right.first)) return true;
        if (compare(right.first, left.first)) return false;
        return left.second < right.second;
    }
};

} // namespace

TEST(SearchDescriptionTest, CustomFourFieldDescriptionControlsParsingAndRangeAvailability) {
    const StudentAttribute<FourFieldKey, FourFieldCompare> attribute{
        static_cast<SearchAttribute>(99), "Student identity and grade",
        [](const Student& student) {
            return FourFieldKey{{student.lastName, student.firstName, student.birthYear}, student.gpa};
        },
        {"Surname", "Given name", "Year", "Grade"},
        [](const SearchKeyInput& fields) -> std::expected<FourFieldKey, Error> {
            const auto year = parseNumber<int>(fields.at(2), "Year");
            if (!year) return std::unexpected(year.error());
            const auto grade = parseNumber<double>(fields.at(3), "Grade");
            if (!grade) return std::unexpected(grade.error());
            return FourFieldKey{{fields.at(0), fields.at(1), *year}, *grade};
        },
        true, "The grade follows the identity fields."
    };
    SearchRequestHandlers handlers;
    handlers.registerAttribute(attribute);
    const auto choices = handlers.getAttributes();
    ASSERT_EQ(choices.size(), 1U);
    EXPECT_EQ(choices[0].id, attribute.id);
    EXPECT_EQ(choices[0].name, attribute.name);
    ASSERT_EQ(choices[0].fieldNames.size(), 4U);
    EXPECT_EQ(choices[0].fieldNames[3], "Grade");
    EXPECT_EQ(choices[0].hint, attribute.hint);
    ASSERT_EQ(handlers.getAttributes(true).size(), 1U);

    StudentSearchService service;
    ASSERT_TRUE(service.addStudent({7, "Anna", "Smith", 2003, 4.5}));
    service.registerIndex(attribute, IndexBackend::SortedArray);
    ASSERT_TRUE(service.buildIndexes({{attribute.id, IndexBackend::SortedArray}}));
    SearchRequest request;
    request.attribute = attribute.id;
    request.lower = {"Smith", "Anna", "2003", "4.5"};
    for (const auto strategy : {ExactSearchStrategy::Linear, ExactSearchStrategy::Binary}) {
        request.exactStrategy = strategy;
        const auto matches = executeSearch(service, request, handlers);
        ASSERT_TRUE(matches);
        ASSERT_EQ(matches->size(), 1U);
        EXPECT_EQ((*matches)[0].recordId, 7U);
    }
    request.range = true;
    request.rangeStrategy = RangeSearchStrategy::Binary;
    request.upper = request.lower;
    EXPECT_EQ(executeSearch(service, request, handlers)->size(), 1U);
    request.upper.pop_back();
    EXPECT_EQ(executeSearch(service, request, handlers).error().code, ErrorCode::InvalidSearchKey);

    auto exactOnly = attribute;
    exactOnly.supportsRange = false;
    handlers.registerAttribute(exactOnly);
    EXPECT_TRUE(handlers.getAttributes(true).empty());
    EXPECT_EQ(executeSearch(service, request, handlers).error().code, ErrorCode::UnknownSelection);
}
