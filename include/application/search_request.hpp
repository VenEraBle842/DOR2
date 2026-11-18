#pragma once

#include "service/student_search_service.hpp"
#include "common/number_input.hpp"

enum class SearchAttribute { Id, LastName, BirthYear, Gpa, CompositeKey };

struct SearchKeyInput {
    std::string value{};
    std::string firstName{};
    std::string birthYear{};
};

struct SearchRequest {
    SearchAttribute attribute{SearchAttribute::Id};
    bool range{false};
    ExactSearchStrategy exactStrategy{ExactSearchStrategy::Linear};
    RangeSearchStrategy rangeStrategy{RangeSearchStrategy::Linear};
    SearchKeyInput lower;
    SearchKeyInput upper;
};

inline std::expected<DynamicArray<RecordId>, AppError> executeSearch(
    const StudentSearchService& service, const SearchRequest& request
) {
    const auto& lower = request.lower;
    const auto& upper = request.upper;
    std::expected<DynamicArray<RecordId>, ServiceError> result;
    switch (request.attribute) {
        case SearchAttribute::Id: {
            if (request.range) return std::unexpected(makeError(AppErrorCode::UnknownSelection, {.field = "Range attribute"}));
            const auto key = parseNumber<RecordId>(lower.value, "ID");
            if (!key) return std::unexpected(key.error());
            result = service.findById(*key, request.exactStrategy);
            break;
        }
        case SearchAttribute::LastName:
            if (request.range) return std::unexpected(makeError(AppErrorCode::UnknownSelection, {.field = "Range attribute"}));
            result = service.findByLastName(lower.value, request.exactStrategy);
            break;
        case SearchAttribute::BirthYear: {
            const auto low = parseNumber<int>(lower.value, "Birth year / lower bound");
            if (!low) return std::unexpected(low.error());
            if (request.range) {
                const auto high = parseNumber<int>(upper.value, "Upper birth year");
                if (!high) return std::unexpected(high.error());
                result = service.findInBirthYearRange(*low, *high, request.rangeStrategy);
            } else result = service.findByBirthYear(*low, request.exactStrategy);
            break;
        }
        case SearchAttribute::Gpa: {
            const auto low = parseNumber<double>(lower.value, "GPA / lower bound");
            if (!low) return std::unexpected(low.error());
            if (request.range) {
                const auto high = parseNumber<double>(upper.value, "Upper GPA");
                if (!high) return std::unexpected(high.error());
                result = service.findInGpaRange(*low, *high, request.rangeStrategy);
            } else result = service.findByGpa(*low, request.exactStrategy);
            break;
        }
        case SearchAttribute::CompositeKey: {
            const auto lowYear = parseNumber<int>(lower.birthYear, "Birth year / lower key");
            if (!lowYear) return std::unexpected(lowYear.error());
            const StudentCompositeKey low{lower.value, lower.firstName, *lowYear};
            if (request.range) {
                const auto highYear = parseNumber<int>(upper.birthYear, "Birth year / upper key");
                if (!highYear) return std::unexpected(highYear.error());
                const StudentCompositeKey high{upper.value, upper.firstName, *highYear};
                result = service.findInCompositeKeyRange(low, high, request.rangeStrategy);
            } else result = service.findByCompositeKey(low, request.exactStrategy);
            break;
        }
        default: return std::unexpected(makeError(AppErrorCode::UnknownSelection, {.field = "Search attribute"}));
    }
    if (!result) return std::unexpected(makeError(result.error()));
    return std::move(*result);
}
