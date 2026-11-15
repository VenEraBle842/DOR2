#include <gtest/gtest.h>

#include "service/student_search_service.hpp"
#include "benchmark/benchmark_runner.hpp"
#include "storage/student_file_storage.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
    for (std::size_t index = 0; index < linear.size(); ++index) {
        EXPECT_EQ(linear[index], tree[index]);
        EXPECT_EQ(linear[index], binary[index]);
    }
    EXPECT_TRUE(service.verifyLastName("Smith").has_value());
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
    for (std::size_t index = 0; index < linear.size(); ++index) EXPECT_EQ(linear[index], binary[index]);
    EXPECT_TRUE(service.verifyBirthYearRange(2002, 2004).has_value());
}

TEST(StudentSearchServiceTest, ReportsUnsupportedAndStaleIndexes) {
    auto service = makeService();
    EXPECT_EQ(service.findById(1, SearchStrategy::Binary)->size(), 1U);
    ASSERT_TRUE(service.addStudent(Student{8, "E", "Smith", 2001, 3.9}).has_value());
    EXPECT_EQ(service.findByLastName("Smith", SearchStrategy::Tree).error(), ServiceError::IndexStale);
    EXPECT_EQ(service.findBirthYearRange(2000, 2005, RangeStrategy::Binary).error(),
              ServiceError::IndexStale);
    EXPECT_EQ(service.findByLastName("Smith", SearchStrategy::Linear)->size(), 4U);
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
