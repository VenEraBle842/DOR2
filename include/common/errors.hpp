#pragma once

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

enum class ErrorCode {
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
static_assert(std::size(errorCatalog) == static_cast<std::size_t>(ErrorCode::Count));

inline const ErrorDescription& describeError(ErrorCode code) {
    const auto index = static_cast<std::size_t>(code);
    return errorCatalog[index < std::size(errorCatalog) ? index : static_cast<std::size_t>(ErrorCode::Unexpected)];
}

struct ErrorContext {
    std::string field{};
    std::filesystem::path path{};
    std::size_t lineNumber{0};
    std::string detail{};
};

struct Error {
    ErrorCode code{ErrorCode::Unexpected};
    ErrorContext context;
};

inline Error makeError(ErrorCode code, ErrorContext context = {}) {
    return {code, std::move(context)};
}

inline std::string formatError(const Error& error) {
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
