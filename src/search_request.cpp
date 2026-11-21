#include "application/search_request.hpp"

SearchRequestHandlers makeDefaultSearchHandlers() {
    SearchRequestHandlers handlers;
    forEachStudentAttribute([&handlers](const auto& attribute) {
        handlers.registerAttribute(attribute);
    });
    return handlers;
}

void SearchRequestHandlers::registerHandler(SearchAttributeDescription description, Handler handler) {
    if (description.fieldNames.empty() || !handler) {
        throw std::invalid_argument("A search attribute requires input fields and a handler");
    }
    for (auto& entry : handlers_) {
        if (entry.description.id == description.id) {
            entry.description = std::move(description);
            entry.handler = std::move(handler);
            return;
        }
    }
    handlers_.push_back({std::move(description), std::move(handler)});
}

DynamicArray<SearchAttributeDescription> SearchRequestHandlers::getAttributes(bool range) const {
    DynamicArray<SearchAttributeDescription> result;
    for (const auto& entry : handlers_) {
        if (!range || entry.description.supportsRange) {
            result.push_back(entry.description);
        }
    }
    return result;
}

std::expected<StudentRecordReferences, Error> SearchRequestHandlers::execute(
    const StudentSearchService& service,
    const SearchRequest& request
) const {
    for (const auto& entry : handlers_) {
        if (entry.description.id != request.attribute) continue;
        const auto& description = entry.description;
        if (request.range && !description.supportsRange) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range attribute"}));
        }
        if (request.lower.size() != description.fieldNames.size() ||
            (request.range && request.upper.size() != description.fieldNames.size())) {
            return std::unexpected(makeError(ErrorCode::InvalidSearchKey,
                {.field = description.name,
                 .detail = "Expected fields per key: " + std::to_string(description.fieldNames.size())}));
        }
        return entry.handler(service, request);
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
