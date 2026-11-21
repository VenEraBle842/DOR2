#pragma once

#include "service/student_search_service.hpp"

#include <functional>
#include <stdexcept>

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

    std::expected<StudentRecordReferences, Error> operator()(
        const StudentSearchService& service,
        const SearchRequest& request
    ) const {
        const auto lower = attribute.parseKey(request.lower);
        if (!lower) return std::unexpected(lower.error());
        if (!request.range) {
            return service.findExact(attribute, *lower, request.exactStrategy);
        }
        const auto upper = attribute.parseKey(request.upper);
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
        SearchAttributeDescription description;
        Handler handler;
    };

    DynamicArray<Registration> handlers_;

public:
    void registerHandler(SearchAttributeDescription description, Handler handler);

    template <typename Key, typename Compare>
    void registerAttribute(const StudentAttribute<Key, Compare>& attribute) {
        if (!attribute.parseKey) {
            throw std::invalid_argument("A search attribute requires a key parser");
        }
        const AttributeSearchHandler<Key, Compare> handler{attribute};
        registerHandler(attribute.getDescription(), handler);
    }

    DynamicArray<SearchAttributeDescription> getAttributes(bool rangeOnly = false) const;

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
