#pragma once

#include "common/errors.hpp"

#include <cmath>
#include <expected>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>

template <typename T>
std::expected<T, Error> parseNumber(const std::string& text, const std::string& field) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::ws;
    if constexpr (std::is_unsigned_v<T>) {
        if (input.peek() == '-') return std::unexpected(makeError(ErrorCode::NegativeNumber, {.field = field}));
    }
    T value{};
    if (!(input >> value)) return std::unexpected(makeError(ErrorCode::InvalidNumber, {.field = field}));
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof()) {
        return std::unexpected(makeError(ErrorCode::InvalidNumber, {.field = field}));
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) return std::unexpected(makeError(ErrorCode::NonFiniteNumber, {.field = field}));
    }
    return value;
}

inline std::expected<std::size_t, Error> parseBoundedSize(
    const std::string& text, const std::string& field, std::size_t maximum
) {
    const auto value = parseNumber<std::size_t>(text, field);
    if (!value) return std::unexpected(value.error());
    if (*value > maximum) {
        return std::unexpected(makeError(ErrorCode::LimitExceeded,
            {.field = field, .detail = "Maximum: " + std::to_string(maximum)}));
    }
    return value;
}
