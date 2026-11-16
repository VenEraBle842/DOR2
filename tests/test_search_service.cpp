#include <gtest/gtest.h>

#include "service/student_search_service.hpp"
#include "benchmark/benchmark_runner.hpp"
#include "storage/student_file_storage.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {

void expectSameIds(const DynamicArray<RecordId>& actual, const DynamicArray<RecordId>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(actual[index], expected[index]) << "at position " << index;
    }
}

} // namespace

StudentSearchService makeService() {
    StudentSearchService service;
    EXPECT_TRUE(service.addStudent(Student{4, "A", "Smith", 2004, 4.1}).has_value());
    EXPECT_TRUE(service.addStudent(Student{2, "B", "Smith", 2002, 4.8}).has_value());
    EXPECT_TRUE(service.addStudent(Student{3, "C", "Brown", 2003, 4.5}).has_value());
    EXPECT_TRUE(service.addStudent(Student{1, "D", "Smith", 2003, 4.5}).has_value());
    service.buildIndexes();
    return service;
}

TEST(StudentSearchServiceTest, ExactStrategiesReturnSameIds) {
    auto service = makeService();
    const auto linear = service.findByLastName("Smith", SearchStrategy::Linear).value();
    const auto tree = service.findByLastName("Smith", SearchStrategy::Tree).value();
    const auto binary = service.findByLastName("Smith", SearchStrategy::Binary).value();
    ASSERT_EQ(linear.size(), 3U);
    expectSameIds(tree, linear);
    expectSameIds(binary, linear);
    for (const auto strategy : {SearchStrategy::Linear, SearchStrategy::Tree, SearchStrategy::Binary}) {
        const auto missing = service.findByLastName("Missing", strategy);
        ASSERT_TRUE(missing.has_value());
        EXPECT_TRUE(missing->empty());
    }
}

TEST(StudentSearchServiceTest, RangeStrategiesReturnKeyThenIdOrder) {
    auto service = makeService();
    const auto linear = service.findBirthYearRange(2002, 2004, RangeStrategy::Linear).value();
    const auto binary = service.findBirthYearRange(2002, 2004, RangeStrategy::Binary).value();
    ASSERT_EQ(linear.size(), 4U);
    EXPECT_EQ(linear[0], 2U);
    EXPECT_EQ(linear[1], 1U);
    EXPECT_EQ(linear[2], 3U);
    EXPECT_EQ(linear[3], 4U);
    expectSameIds(binary, linear);
    for (const auto strategy : {RangeStrategy::Linear, RangeStrategy::Binary}) {
        EXPECT_EQ(service.findBirthYearRange(2004, 2002, strategy).error(), ServiceError::InvalidRange);
    }
}

TEST(StudentSearchServiceTest, ReportsStaleIndexes) {
    auto service = makeService();
    EXPECT_EQ(service.findById(1, SearchStrategy::Binary)->size(), 1U);
    ASSERT_TRUE(service.addStudent(Student{8, "E", "Smith", 2001, 3.9}).has_value());
    EXPECT_EQ(service.findByLastName("Smith", SearchStrategy::Tree).error(), ServiceError::IndexStale);
    EXPECT_EQ(service.findBirthYearRange(2000, 2005, RangeStrategy::Binary).error(),
              ServiceError::IndexStale);
    EXPECT_EQ(service.findByLastName("Smith", SearchStrategy::Linear)->size(), 4U);
}

TEST(StudentSearchServiceTest, BinaryRangesPreserveCatalogErrorsAndTheirPriority) {
    StudentSearchService service;
    for (const auto error : {ServiceError::IndexNotBuilt, ServiceError::IndexStale}) {
        if (error == ServiceError::IndexStale) {
            service.buildIndexes();
            ASSERT_TRUE(service.addStudent(Student{1, "A", "Smith", 2003, 4.5}).has_value());
        }
        EXPECT_EQ(service.findBirthYearRange(2002, 2004, RangeStrategy::Binary).error(), error);
        EXPECT_EQ(service.findGpaRange(4.0, 5.0, RangeStrategy::Binary).error(), error);
        EXPECT_EQ(service.findCompositeRange(
            {"Brown", "", 0}, {"Smith", "Z", 9999}, RangeStrategy::Binary
        ).error(), error);
        EXPECT_EQ(service.findBirthYearRange(2004, 2002, RangeStrategy::Binary).error(), error);
        EXPECT_EQ(service.findGpaRange(5.0, 4.0, RangeStrategy::Binary).error(), error);
        EXPECT_EQ(service.findCompositeRange(
            {"Smith", "Z", 9999}, {"Brown", "", 0}, RangeStrategy::Binary
        ).error(), error);
        EXPECT_EQ(service.findBirthYearRange(2002, 2004, RangeStrategy::Linear)->size(),
                  service.getRepository().getCount());
    }
    service.buildIndexes();
    EXPECT_EQ(service.findBirthYearRange(2004, 2002, RangeStrategy::Binary).error(),
              ServiceError::InvalidRange);
    EXPECT_EQ(service.findGpaRange(5.0, 4.0, RangeStrategy::Binary).error(),
              ServiceError::InvalidRange);
    EXPECT_EQ(service.findCompositeRange(
        {"Smith", "Z", 9999}, {"Brown", "", 0}, RangeStrategy::Binary
    ).error(), ServiceError::InvalidRange);
}

TEST(StudentSearchServiceTest, NonFiniteRangeBoundsTakePriorityOverIndexState) {
    StudentSearchService service;
    for (int state = 0; state < 3; ++state) {
        if (state == 1) service.buildIndexes();
        if (state == 2) {
            ASSERT_TRUE(service.addStudent(Student{1, "A", "Smith", 2003, 4.5}).has_value());
        }
        for (const auto strategy : {RangeStrategy::Linear, RangeStrategy::Binary}) {
            for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                         std::numeric_limits<double>::infinity(),
                                         -std::numeric_limits<double>::infinity()}) {
                EXPECT_EQ(service.findGpaRange(invalid, 5.0, strategy).error(),
                          ServiceError::InvalidKey);
                EXPECT_EQ(service.findGpaRange(4.0, invalid, strategy).error(),
                          ServiceError::InvalidKey);
            }
        }
    }
}

TEST(StudentSearchServiceTest, GpaAndCompositeExactStrategiesAgreeIncludingDuplicateKeys) {
    auto service = makeService();
    ASSERT_TRUE(service.addStudent(Student{8, "D", "Smith", 2003, 4.5}).has_value());
    service.buildIndexes();
    for (const auto strategy : {SearchStrategy::Linear, SearchStrategy::Tree, SearchStrategy::Binary}) {
        const auto gpa = service.findByGpa(4.5, strategy);
        ASSERT_TRUE(gpa.has_value());
        expectSameIds(*gpa, {1, 3, 8});
        const auto composite = service.findByComposite({"Smith", "D", 2003}, strategy);
        ASSERT_TRUE(composite.has_value());
        expectSameIds(*composite, {1, 8});
        const auto missingGpa = service.findByGpa(0.0, strategy);
        ASSERT_TRUE(missingGpa.has_value());
        EXPECT_TRUE(missingGpa->empty());
        const auto missingComposite = service.findByComposite({"Smith", "D", 2002}, strategy);
        ASSERT_TRUE(missingComposite.has_value());
        EXPECT_TRUE(missingComposite->empty());
    }
}

TEST(StudentSearchServiceTest, GpaAndCompositeRangesAgreeAndIncludeBothBounds) {
    auto service = makeService();
    ASSERT_TRUE(service.addStudent(Student{8, "D", "Smith", 2003, 4.5}).has_value());
    service.buildIndexes();
    for (const auto strategy : {RangeStrategy::Linear, RangeStrategy::Binary}) {
        const auto gpa = service.findGpaRange(4.1, 4.5, strategy);
        ASSERT_TRUE(gpa.has_value());
        expectSameIds(*gpa, {4, 1, 3, 8});
        const auto composite = service.findCompositeRange(
            {"Smith", "B", 2002}, {"Smith", "D", 2003}, strategy
        );
        ASSERT_TRUE(composite.has_value());
        expectSameIds(*composite, {2, 1, 8});
        const auto point = service.findCompositeRange(
            {"Smith", "D", 2003}, {"Smith", "D", 2003}, strategy
        );
        ASSERT_TRUE(point.has_value());
        expectSameIds(*point, {1, 8});
        const auto empty = service.findCompositeRange(
            {"Smith", "B", 2003}, {"Smith", "C", 2004}, strategy
        );
        ASSERT_TRUE(empty.has_value());
        EXPECT_TRUE(empty->empty());
        EXPECT_EQ(service.findCompositeRange(
            {"Smith", "D", 2003}, {"Smith", "B", 2002}, strategy
        ).error(), ServiceError::InvalidRange);
    }
}

TEST(StudentSearchServiceTest, SupportsCompositeAndGpaQueries) {
    auto service = makeService();
    EXPECT_EQ(service.findByGpa(4.5, SearchStrategy::Binary)->size(), 2U);
    EXPECT_EQ(service.findByComposite(
        StudentCompositeKey{"Smith", "D", 2003}, SearchStrategy::Tree
    )->size(), 1U);
    EXPECT_EQ(service.findGpaRange(4.8, 4.0, RangeStrategy::Linear).error(),
              ServiceError::InvalidRange);
    const auto compositeRange = service.findCompositeRange(
        StudentCompositeKey{"Brown", "", 0},
        StudentCompositeKey{"Smith", "Z", 9999},
        RangeStrategy::Binary
    );
    ASSERT_TRUE(compositeRange.has_value());
    EXPECT_EQ(compositeRange->size(), 4U);
}

TEST(StorageSearchIntegrationTest, LoadBuildSearchAndSaveCycle) {
    const auto base = std::filesystem::temp_directory_path() /
        ("dor2-integration-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
    auto input = base;
    input += "-input.txt";
    auto output = base;
    output += "-output.txt";

    StudentRepository source;
    ASSERT_TRUE(source.add(Student{11, "One", "Shared", 2001, 4.1}).has_value());
    ASSERT_TRUE(source.add(Student{12, "Two", "Shared", 2002, 4.2}).has_value());
    ASSERT_TRUE(StudentFileStorage::save(input, source).has_value());

    auto loaded = StudentFileStorage::load(input);
    ASSERT_TRUE(loaded.has_value());
    StudentSearchService service;
    ASSERT_TRUE(service.replaceStudents(std::move(loaded.value())).has_value());
    service.buildIndexes();
    ASSERT_EQ(service.findByLastName("Shared", SearchStrategy::Tree)->size(), 2U);
    ASSERT_TRUE(StudentFileStorage::save(output, service.getRepository()).has_value());
    ASSERT_EQ(StudentFileStorage::load(output)->size(), 2U);

    std::error_code ignored;
    std::filesystem::remove(input, ignored);
    std::filesystem::remove(output, ignored);
}

TEST(BenchmarkIntegrationTest, SmallRunIsVerifiedAndWritesCsv) {
    const auto path = std::filesystem::temp_directory_path() /
        ("dor2-benchmark-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ) + ".csv");
    const auto results = BenchmarkRunner::runAndSave(path, 20, 10, 123);
    ASSERT_TRUE(results.has_value());
    ASSERT_EQ(results->size(), 21U);
    for (const BenchmarkResult& result : *results) EXPECT_TRUE(result.verified);
    std::ifstream input(path);
    std::string header;
    ASSERT_TRUE(static_cast<bool>(std::getline(input, header)));
    EXPECT_EQ(header, "DatasetSize,InputOrder,Algorithm,Operation,QueryCount,ElapsedUs,MatchCount,Seed,Verified");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
