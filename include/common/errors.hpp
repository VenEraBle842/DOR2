#pragma once

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

enum class DictionaryError { DuplicateKey };
enum class SequenceError { InvalidRange };
enum class RepositoryError { DuplicateId, InvalidGpa };
enum class IndexError { NotBuilt, Stale, InvalidKey, InvalidRange };
enum class ServiceError { IndexNotBuilt, IndexStale, InvalidRange, InvalidKey, UnexpectedIndexError };
enum class BenchmarkError { InvalidWorkload, PreparationFailed, FileOperationFailed };

enum class FileError {
    OpenFailed,
    ReadFailed,
    WriteFailed,
    InvalidHeader,
    UnsupportedVersion,
    InvalidRecord,
    RecordCountMismatch,
    TrailingData,
    DuplicateId
};

struct FileFailure {
    FileError code{FileError::ReadFailed};
    std::string message;
    std::filesystem::path path{};
    std::size_t lineNumber{0};
};

enum class AppErrorCode {
#define DOR2_ERROR(code, message, hint) code,
#include "common/error_catalog.def"
#undef DOR2_ERROR
    Count
};

struct ErrorDescription {
    std::string_view name;
    std::string_view message;
    std::string_view hint;
};

inline constexpr ErrorDescription errorCatalog[] = {
#define DOR2_ERROR(code, message, hint) {#code, message, hint},
#include "common/error_catalog.def"
#undef DOR2_ERROR
};
static_assert(std::size(errorCatalog) == static_cast<std::size_t>(AppErrorCode::Count));

inline const ErrorDescription& describeError(AppErrorCode code) {
    const auto index = static_cast<std::size_t>(code);
    return errorCatalog[index < std::size(errorCatalog) ? index : static_cast<std::size_t>(AppErrorCode::Unexpected)];
}

struct ErrorContext {
    std::string field{};
    std::filesystem::path path{};
    std::size_t lineNumber{0};
    std::string detail{};
};

struct AppError {
    AppErrorCode code{AppErrorCode::Unexpected};
    ErrorContext context;
};

inline AppError makeError(AppErrorCode code, ErrorContext context = {}) {
    return {code, std::move(context)};
}

inline AppError makeError(RepositoryError error, ErrorContext context = {}) {
    switch (error) {
        case RepositoryError::DuplicateId: return makeError(AppErrorCode::DuplicateId, std::move(context));
        case RepositoryError::InvalidGpa: return makeError(AppErrorCode::NonFiniteNumber, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(DictionaryError error, ErrorContext context = {}) {
    switch (error) {
        case DictionaryError::DuplicateKey: return makeError(AppErrorCode::DuplicateKey, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(SequenceError error, ErrorContext context = {}) {
    switch (error) {
        case SequenceError::InvalidRange: return makeError(AppErrorCode::InvalidRange, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(BenchmarkError error, ErrorContext context = {}) {
    switch (error) {
        case BenchmarkError::InvalidWorkload: return makeError(AppErrorCode::InvalidWorkload, std::move(context));
        case BenchmarkError::PreparationFailed: return makeError(AppErrorCode::BenchmarkPreparation, std::move(context));
        case BenchmarkError::FileOperationFailed: return makeError(AppErrorCode::FileWrite, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(IndexError error, ErrorContext context = {}) {
    switch (error) {
        case IndexError::NotBuilt: return makeError(AppErrorCode::IndexNotBuilt, std::move(context));
        case IndexError::Stale: return makeError(AppErrorCode::IndexStale, std::move(context));
        case IndexError::InvalidKey: return makeError(AppErrorCode::NonFiniteNumber, std::move(context));
        case IndexError::InvalidRange: return makeError(AppErrorCode::InvalidRange, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(ServiceError error, ErrorContext context = {}) {
    switch (error) {
        case ServiceError::IndexNotBuilt: return makeError(AppErrorCode::IndexNotBuilt, std::move(context));
        case ServiceError::IndexStale: return makeError(AppErrorCode::IndexStale, std::move(context));
        case ServiceError::InvalidKey: return makeError(AppErrorCode::NonFiniteNumber, std::move(context));
        case ServiceError::InvalidRange: return makeError(AppErrorCode::InvalidRange, std::move(context));
        case ServiceError::UnexpectedIndexError: return makeError(AppErrorCode::Unexpected, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(FileError error, ErrorContext context = {}) {
    switch (error) {
        case FileError::OpenFailed: return makeError(AppErrorCode::FileOpen, std::move(context));
        case FileError::ReadFailed: return makeError(AppErrorCode::FileRead, std::move(context));
        case FileError::WriteFailed: return makeError(AppErrorCode::FileWrite, std::move(context));
        case FileError::InvalidHeader: return makeError(AppErrorCode::FileHeader, std::move(context));
        case FileError::UnsupportedVersion: return makeError(AppErrorCode::FileVersion, std::move(context));
        case FileError::InvalidRecord: return makeError(AppErrorCode::FileRecord, std::move(context));
        case FileError::RecordCountMismatch: return makeError(AppErrorCode::FileCount, std::move(context));
        case FileError::TrailingData: return makeError(AppErrorCode::FileTrailingData, std::move(context));
        case FileError::DuplicateId: return makeError(AppErrorCode::DuplicateId, std::move(context));
    }
    return makeError(AppErrorCode::Unexpected, std::move(context));
}

inline AppError makeError(const FileFailure& failure) {
    return makeError(failure.code, {.path = failure.path, .lineNumber = failure.lineNumber, .detail = failure.message});
}

inline std::string formatError(const AppError& error) {
    const auto& description = describeError(error.code);
    std::string result(description.message);
    const auto& context = error.context;
    if (!context.field.empty()) result += "\nField: " + context.field;
    if (!context.path.empty()) result += "\nFile: " + context.path.string();
    if (context.lineNumber != 0) result += "\nLine: " + std::to_string(context.lineNumber);
    if (!context.detail.empty()) result += "\nDetails: " + context.detail;
    if (!description.hint.empty()) result += "\n" + std::string(description.hint);
    return result;
}

template <typename Error>
std::string toString(Error error) {
    return std::string(describeError(makeError(error).code).message);
}
