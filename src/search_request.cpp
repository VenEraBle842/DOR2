#include "application/search_request.hpp"
#include "common/number_input.hpp"

std::expected<RecordId, Error> parseIdKey(const SearchKeyInput& input) {
    return parseNumber<RecordId>(input.value, "ID");
}

std::expected<std::string, Error> parseLastNameKey(const SearchKeyInput& input) {
    return input.value;
}

std::expected<int, Error> parseBirthYearKey(const SearchKeyInput& input) {
    return parseNumber<int>(input.value, "Birth year / bound");
}

std::expected<double, Error> parseGpaKey(const SearchKeyInput& input) {
    return parseNumber<double>(input.value, "GPA / bound");
}

std::expected<StudentCompositeKey, Error> parseCompositeKey(const SearchKeyInput& input) {
    const auto year = parseNumber<int>(input.birthYear, "Birth year / key");
    if (!year) return std::unexpected(year.error());
    return StudentCompositeKey{input.value, input.firstName, *year};
}

SearchRequestHandlers makeDefaultSearchHandlers() {
    SearchRequestHandlers handlers;
    handlers.registerAttribute(studentIdAttribute, parseIdKey, false);
    handlers.registerAttribute(studentLastNameAttribute, parseLastNameKey, false);
    handlers.registerAttribute(studentBirthYearAttribute, parseBirthYearKey, true);
    handlers.registerAttribute(studentGpaAttribute, parseGpaKey, true);
    handlers.registerAttribute(studentCompositeAttribute, parseCompositeKey, true);
    return handlers;
}

void SearchRequestHandlers::registerHandler(SearchAttribute attribute, Handler handler) {
    for (auto& entry : handlers_) {
        if (entry.attribute == attribute) {
            entry.handler = std::move(handler);
            return;
        }
    }
    handlers_.push_back({attribute, std::move(handler)});
}

std::expected<StudentRecordReferences, Error> SearchRequestHandlers::execute(
    const StudentSearchService& service,
    const SearchRequest& request
) const {
    for (const auto& entry : handlers_) {
        if (entry.attribute == request.attribute) {
            return entry.handler(service, request);
        }
    }
    return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Search attribute"}));
}

const SearchRequestHandlers& defaultSearchHandlers() {
    static const SearchRequestHandlers handlers = makeDefaultSearchHandlers();
    return handlers;
}

std::expected<StudentRecordReferences, Error> executeSearch(
    const StudentSearchService& service,
    const SearchRequest& request,
    const SearchRequestHandlers& handlers
) {
    return handlers.execute(service, request);
}
