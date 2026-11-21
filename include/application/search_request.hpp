#pragma once

#include "service/student_search_service.hpp"

#include <functional>
#include <utility>

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

template <typename Key, typename Compare>
struct AttributeSearchHandler {
    StudentAttribute<Key, Compare> attribute;
    std::function<std::expected<Key, Error>(const SearchKeyInput&)> parseKey;
    bool supportsRange{false};

    std::expected<StudentRecordReferences, Error> operator()(
        const StudentSearchService& service,
        const SearchRequest& request
    ) const {
        if (request.range && !supportsRange) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Range attribute"}));
        }
        const auto lower = parseKey(request.lower);
        if (!lower) return std::unexpected(lower.error());
        if (!request.range) {
            return service.findExact(attribute, *lower, request.exactStrategy);
        }
        const auto upper = parseKey(request.upper);
        if (!upper) return std::unexpected(upper.error());
        return service.findRange(attribute, *lower, *upper, request.rangeStrategy);
    }
};

class SearchRequestHandlers {
public:
    using Handler = std::function<std::expected<StudentRecordReferences, Error>(
        const StudentSearchService&, const SearchRequest&)>;

private:
    struct Registration {
        SearchAttribute attribute{SearchAttribute::Id};
        Handler handler;
    };

    DynamicArray<Registration> handlers_;

public:
    void registerHandler(SearchAttribute attribute, Handler handler);

    template <typename Key, typename Compare, typename Parser>
    void registerAttribute(
        const StudentAttribute<Key, Compare>& attribute,
        Parser parser,
        bool supportsRange
    ) {
        const AttributeSearchHandler<Key, Compare> handler{attribute, std::move(parser), supportsRange};
        registerHandler(attribute.id, handler);
    }

    std::expected<StudentRecordReferences, Error> execute(
        const StudentSearchService& service,
        const SearchRequest& request
    ) const;
};

const SearchRequestHandlers& defaultSearchHandlers();

std::expected<StudentRecordReferences, Error> executeSearch(
    const StudentSearchService& service,
    const SearchRequest& request,
    const SearchRequestHandlers& handlers = defaultSearchHandlers()
);
