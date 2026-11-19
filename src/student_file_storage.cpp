#include "storage/student_file_storage.hpp"

#include "dictionary/tree_dictionary.hpp"

#include <cctype>
#include <fstream>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>

bool hasNonWhitespace(const std::string& text) {
    for (const char character : text) {
        if (!std::isspace(static_cast<unsigned char>(character))) return true;
    }
    return false;
}
std::expected<DynamicArray<Student>, Error> StudentFileStorage::load(
    const std::filesystem::path& path
) {
    std::ifstream input(path);
    input.imbue(std::locale::classic());
    if (!input.is_open()) {
        return std::unexpected(makeError(ErrorCode::FileOpen,
            {.path = path, .detail = "cannot open input file"}));
    }

    std::string line;
    if (!std::getline(input, line)) {
        return std::unexpected(makeError(ErrorCode::FileHeader,
            {.path = path, .lineNumber = 1, .detail = "missing format header"}));
    }

    std::istringstream header(line);
    header.imbue(std::locale::classic());
    std::string signature;
    int version = 0;
    if (!(header >> signature >> version) || signature != "DOR2_STUDENT_DATA") {
        return std::unexpected(makeError(ErrorCode::FileHeader,
            {.path = path, .lineNumber = 1, .detail = "expected DOR2_STUDENT_DATA header"}));
    }
    header >> std::ws;
    if (header.peek() != std::char_traits<char>::eof()) {
        return std::unexpected(makeError(ErrorCode::FileHeader,
            {.path = path, .lineNumber = 1, .detail = "unexpected header fields"}));
    }
    if (version != formatVersion) {
        return std::unexpected(makeError(ErrorCode::FileVersion,
            {.path = path, .lineNumber = 1, .detail = "unsupported data version"}));
    }

    if (!std::getline(input, line)) {
        return std::unexpected(makeError(ErrorCode::FileCount,
            {.path = path, .lineNumber = 2, .detail = "missing record count"}));
    }
    std::istringstream countStream(line);
    countStream.imbue(std::locale::classic());
    std::size_t expectedCount = 0;
    if (!(countStream >> expectedCount)) {
        return std::unexpected(makeError(ErrorCode::FileCount,
            {.path = path, .lineNumber = 2, .detail = "invalid record count"}));
    }
    countStream >> std::ws;
    if (countStream.peek() != std::char_traits<char>::eof()) {
        return std::unexpected(makeError(ErrorCode::FileCount,
            {.path = path, .lineNumber = 2, .detail = "unexpected count fields"}));
    }

    DynamicArray<Student> students;
    TreeDictionary<std::uint64_t, bool> seenIds;
    for (std::size_t index = 0; index < expectedCount; ++index) {
        const std::size_t lineNumber = index + 3;
        if (!std::getline(input, line)) {
            return std::unexpected(makeError(ErrorCode::FileCount,
                {.path = path, .lineNumber = lineNumber, .detail = "fewer records than declared"}));
        }
        std::istringstream recordStream(line);
        recordStream.imbue(std::locale::classic());
        Student student;
        if (!(recordStream >> student)) {
            return std::unexpected(makeError(ErrorCode::FileRecord,
                {.path = path, .lineNumber = lineNumber, .detail = "invalid student record"}));
        }
        recordStream >> std::ws;
        if (recordStream.peek() != std::char_traits<char>::eof()) {
            return std::unexpected(makeError(ErrorCode::FileRecord,
                {.path = path, .lineNumber = lineNumber, .detail = "trailing record data"}));
        }
        if (!seenIds.add(student.id, true)) {
            return std::unexpected(makeError(ErrorCode::DuplicateId,
                {.path = path, .lineNumber = lineNumber, .detail = "duplicate student id"}));
        }
        students.push_back(std::move(student));
    }

    std::size_t lineNumber = expectedCount + 3;
    while (std::getline(input, line)) {
        if (hasNonWhitespace(line)) {
            return std::unexpected(makeError(ErrorCode::FileTrailingData,
                {.path = path, .lineNumber = lineNumber, .detail = "unexpected data after records"}));
        }
        ++lineNumber;
    }
    if (input.bad()) {
        return std::unexpected(makeError(ErrorCode::FileRead,
            {.path = path, .lineNumber = lineNumber, .detail = "input stream failure"}));
    }
    return students;
}

std::expected<void, Error> StudentFileStorage::save(
    const std::filesystem::path& path,
    const StudentRepository& repository
) {
    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";

    std::ofstream output(temporaryPath, std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output.is_open()) {
        return std::unexpected(makeError(ErrorCode::FileOpen,
            {.path = path, .detail = "cannot open temporary output file"}));
    }

    output << "DOR2_STUDENT_DATA " << formatVersion << '\n';
    output << repository.getCount() << '\n';
    for (std::size_t position = 0; position < repository.getCount(); ++position) {
        output << repository.at(position) << '\n';
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return std::unexpected(makeError(ErrorCode::FileWrite,
                {.path = path, .detail = "failed while writing records"}));
        }
    }
    output.flush();
    if (!output) {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeError(ErrorCode::FileWrite,
            {.path = path, .detail = "failed to flush output"}));
    }
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return std::unexpected(makeError(ErrorCode::FileWrite,
            {.path = path, .detail = "failed to close output"}));
    }

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
