#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

enum class DictionaryError { DuplicateKey };
enum class SequenceError { InvalidRange };
enum class RepositoryError { DuplicateId, InvalidGpa };
enum class IndexError { NotBuilt, Stale, InvalidKey, InvalidRange };

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
    std::filesystem::path path;
    std::size_t lineNumber{0};
};

inline std::string toString(RepositoryError error) {
    switch (error) {
        case RepositoryError::DuplicateId: return "duplicate student ID";
        case RepositoryError::InvalidGpa: return "GPA must be finite";
    }
    return "unknown repository error";
}

inline std::string toString(IndexError error) {
    switch (error) {
        case IndexError::NotBuilt: return "indexes are not built";
        case IndexError::Stale: return "indexes are stale";
        case IndexError::InvalidKey: return "GPA must be finite";
        case IndexError::InvalidRange: return "range upper bound is below lower bound";
    }
    return "unknown index error";
}

inline std::string toString(FileError error) {
    switch (error) {
        case FileError::OpenFailed: return "open failed";
        case FileError::ReadFailed: return "read failed";
        case FileError::WriteFailed: return "write failed";
        case FileError::InvalidHeader: return "invalid header";
        case FileError::UnsupportedVersion: return "unsupported version";
        case FileError::InvalidRecord: return "invalid record";
        case FileError::RecordCountMismatch: return "record count mismatch";
        case FileError::TrailingData: return "trailing data";
        case FileError::DuplicateId: return "duplicate id";
    }
    return "unknown file error";
}
