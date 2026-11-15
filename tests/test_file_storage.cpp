#include <gtest/gtest.h>

#include "storage/student_file_storage.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

class StorageTest : public testing::Test {
protected:
    std::filesystem::path path_ = std::filesystem::temp_directory_path() /
        ("dor2-storage-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ) + ".txt");

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        std::filesystem::remove(path_.string() + ".tmp", ignored);
    }

    void write(const std::string& content) {
        std::ofstream output(path_);
        output << content;
    }
};

TEST_F(StorageTest, SaveLoadRoundTripIncludingEscapedText) {
    StudentRepository repository;
    ASSERT_TRUE(repository.add(Student{1, "A B", "Q\"R:Z\\X", 2001, 4.75}).has_value());
    ASSERT_TRUE(StudentFileStorage::save(path_, repository).has_value());
    const auto loaded = StudentFileStorage::load(path_);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), 1U);
    EXPECT_EQ((*loaded)[0], repository.at(0));
}

TEST_F(StorageTest, SupportsEmptyDataSet) {
    StudentRepository repository;
    ASSERT_TRUE(StudentFileStorage::save(path_, repository).has_value());
    const auto loaded = StudentFileStorage::load(path_);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->empty());
}

TEST_F(StorageTest, RejectsHeaderVersionCountAndTrailingData) {
    write("BAD 1\n0\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::InvalidHeader);
    write("DOR2_STUDENT_DATA 9\n0\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::UnsupportedVersion);
    write("DOR2_STUDENT_DATA 1\n2\n1:\"A\":\"B\":2000:4\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::RecordCountMismatch);
    write("DOR2_STUDENT_DATA 1\n0\nextra\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::TrailingData);
}

TEST_F(StorageTest, RejectsInvalidDuplicateAndNonFiniteRecords) {
    write("DOR2_STUDENT_DATA 1\n1\ninvalid\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::InvalidRecord);
    write("DOR2_STUDENT_DATA 1\n2\n1:\"A\":\"B\":2000:4\n1:\"C\":\"D\":2001:5\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::DuplicateId);
    write("DOR2_STUDENT_DATA 1\n1\n1:\"A\":\"B\":2000:nan\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::InvalidRecord);
    write("DOR2_STUDENT_DATA 1\n1\n1:\"A\":\"B\":2000:inf\n");
    EXPECT_EQ(StudentFileStorage::load(path_).error().code, FileError::InvalidRecord);
}

TEST_F(StorageTest, FailedLoadDoesNotChangeRepository) {
    StudentRepository repository;
    ASSERT_TRUE(repository.add(Student{7, "Keep", "Me", 2000, 4.0}).has_value());
    write("DOR2_STUDENT_DATA 1\n1\nbroken\n");
    EXPECT_FALSE(StudentFileStorage::loadInto(path_, repository).has_value());
    ASSERT_EQ(repository.getCount(), 1U);
    EXPECT_EQ(repository.at(0).id, 7U);
}

TEST_F(StorageTest, ReportsOpenFailure) {
    EXPECT_EQ(StudentFileStorage::load(path_ / "missing").error().code, FileError::OpenFailed);
    StudentRepository repository;
    EXPECT_EQ(StudentFileStorage::save(path_ / "missing", repository).error().code,
              FileError::OpenFailed);
}
