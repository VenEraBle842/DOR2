#include "index/student_attributes.hpp"
#include "common/number_input.hpp"

std::expected<RecordId, Error> parseIdKey(const SearchKeyInput& input) {
    return parseNumber<RecordId>(input.at(0), "ID");
}

std::expected<std::string, Error> parseLastNameKey(const SearchKeyInput& input) {
    return input.at(0);
}

std::expected<int, Error> parseBirthYearKey(const SearchKeyInput& input) {
    return parseNumber<int>(input.at(0), "Birth year");
}

std::expected<double, Error> parseGpaKey(const SearchKeyInput& input) {
    return parseNumber<double>(input.at(0), "GPA");
}

std::expected<StudentCompositeKey, Error> parseCompositeKey(const SearchKeyInput& input) {
    const auto year = parseNumber<int>(input.at(2), "Birth year");
    if (!year) return std::unexpected(year.error());
    return StudentCompositeKey{input.at(0), input.at(1), *year};
}
