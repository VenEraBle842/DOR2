#include "storage/student_file_storage.hpp"

#include "dictionary/tree_dictionary.hpp"

#include <cctype>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>

FileFailure makeFileFailure(
    FileError code,
    const std::filesystem::path& path,
    std::string message,
    std::size_t lineNumber = 0
) {
    return FileFailure{code, std::move(message), path, lineNumber};
}

bool hasNonWhitespace(const std::string& text) {
    for (const char character : text) {
        if (!std::isspace(static_cast<unsigned char>(character))) return true;
    }
    return false;
}
std::expected<DynamicArray<Student>, FileFailure> StudentFileStorage::load(
    const std::filesystem::path& path
) {
    std::ifstream input(path);
    input.imbue(std::locale::classic());
    if (!input.is_open()) {
        return std::unexpected(makeFileFailure(FileError::OpenFailed, path, "cannot open input file"));
    }

    std::string line;
    if (!std::getline(input, line)) {
        return std::unexpected(makeFileFailure(FileError::InvalidHeader, path, "missing format header", 1));
    }

    std::istringstream header(line);
    header.imbue(std::locale::classic());
    std::string signature;
    int version = 0;
    if (!(header >> signature >> version) || signature != "DOR2_STUDENT_DATA") {
        return std::unexpected(makeFileFailure(FileError::InvalidHeader, path, "expected DOR2_STUDENT_DATA header", 1));
    }
    header >> std::ws;
    if (header.peek() != std::char_traits<char>::eof()) {
        return std::unexpected(makeFileFailure(FileError::InvalidHeader, path, "unexpected header fields", 1));
    }
    if (version != formatVersion) {
        return std::unexpected(makeFileFailure(FileError::UnsupportedVersion, path, "unsupported data version", 1));
    }

    if (!std::getline(input, line)) {
        return std::unexpected(makeFileFailure(FileError::RecordCountMismatch, path, "missing record count", 2));
    }
    std::istringstream countStream(line);
    countStream.imbue(std::locale::classic());
    std::size_t expectedCount = 0;
    if (!(countStream >> expectedCount)) {
        return std::unexpected(makeFileFailure(FileError::RecordCountMismatch, path, "invalid record count", 2));
    }
    countStream >> std::ws;
    if (countStream.peek() != std::char_traits<char>::eof()) {
        return std::unexpected(makeFileFailure(FileError::RecordCountMismatch, path, "unexpected count fields", 2));
    }

    DynamicArray<Student> students;
    TreeDictionary<std::uint64_t, bool> seenIds;
    for (std::size_t index = 0; index < expectedCount; ++index) {
        const std::size_t lineNumber = index + 3;
        if (!std::getline(input, line)) {
            return std::unexpected(makeFileFailure(
                FileError::RecordCountMismatch, path, "fewer records than declared", lineNumber
            ));
        }
        std::istringstream recordStream(line);
        recordStream.imbue(std::locale::classic());
        Student student;
        if (!(recordStream >> student)) {
            return std::unexpected(makeFileFailure(FileError::InvalidRecord, path, "invalid student record", lineNumber));
        }
        recordStream >> std::ws;
        if (recordStream.peek() != std::char_traits<char>::eof()) {
            return std::unexpected(makeFileFailure(FileError::InvalidRecord, path, "trailing record data", lineNumber));
        }
        if (!seenIds.add(student.id, true)) {
            return std::unexpected(makeFileFailure(FileError::DuplicateId, path, "duplicate student id", lineNumber));
        }
        students.push_back(std::move(student));
    }

    std::size_t lineNumber = expectedCount + 3;
    while (std::getline(input, line)) {
        if (hasNonWhitespace(line)) {
            return std::unexpected(makeFileFailure(FileError::TrailingData, path, "unexpected data after records", lineNumber));
        }
        ++lineNumber;
    }
    if (input.bad()) {
        return std::unexpected(makeFileFailure(FileError::ReadFailed, path, "input stream failure", lineNumber));
    }
    return students;
}

std::expected<void, FileFailure> StudentFileStorage::save(
    const std::filesystem::path& path,
    const StudentRepository& repository
) {
    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";

    std::ofstream output(temporaryPath, std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output.is_open()) {
        return std::unexpected(makeFileFailure(FileError::OpenFailed, path, "cannot open temporary output file"));
    }

    output << "DOR2_STUDENT_DATA " << formatVersion << '\n';
    output << repository.getCount() << '\n';
    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        output << repository.at(position) << '\n';
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected(makeFileFailure(FileError::WriteFailed, path, "failed while writing records"));
        }
    }
    output.flush();
    if (!output) {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeFileFailure(FileError::WriteFailed, path, "failed to flush output"));
    }
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeFileFailure(FileError::WriteFailed, path, "failed to close output"));
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryPath, path, renameError);
    if (renameError) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeFileFailure(FileError::WriteFailed, path, renameError.message()));
    }
    return {};
}
