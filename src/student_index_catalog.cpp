#include "index/student_index_catalog.hpp"

StudentIndexCatalog::StudentIndexCatalog() {
    forEachDefaultStudentAttribute([this](const auto& attribute) {
        registerIndex(attribute, IndexBackend::Tree);
        registerIndex(attribute, IndexBackend::SortedArray);
    });
}

StudentIndexCatalog::StudentIndexCatalog(const StudentIndexCatalog& other) {
    for (const auto& entry : other.indexes_) {
        std::shared_ptr<IStudentIndexBase> copy = entry.index->clone();
        indexes_.push_back({entry.selection, entry.name, std::move(copy)});
    }
}

StudentIndexCatalog& StudentIndexCatalog::operator=(const StudentIndexCatalog& other) {
    if (this != &other) {
        StudentIndexCatalog copy(other);
        *this = std::move(copy);
    }
    return *this;
}

std::shared_ptr<const IStudentIndexBase> StudentIndexCatalog::getIndex(
    IndexSelection selection
) const noexcept {
    for (const auto& entry : indexes_) {
        if (entry.selection == selection) return entry.index;
    }
    return nullptr;
}

DynamicArray<IndexSelection> StudentIndexCatalog::getSelections() const {
    DynamicArray<IndexSelection> result;
    for (const auto& entry : indexes_) {
        result.push_back(entry.selection);
    }
    return result;
}

DynamicArray<StudentIndexState> StudentIndexCatalog::getStates(
    const StudentRepository& repository
) const {
    DynamicArray<StudentIndexState> result;
    for (const auto& entry : indexes_) {
        result.push_back({
            entry.selection, entry.name, entry.index->isBuilt(),
            entry.index->isStale(repository), entry.index->getKeyCount()
        });
    }
    return result;
}

std::expected<void, Error> StudentIndexCatalog::build(
    const StudentRepository& repository,
    const DynamicArray<IndexSelection>& selections
) {
    struct IndexReplacement {
        IndexSelection selection;
        std::shared_ptr<IStudentIndexBase> index;
    };

    DynamicArray<IndexReplacement> replacements;
    for (const auto selection : selections) {
        const auto current = getIndex(selection);
        if (!current) {
            return std::unexpected(makeError(ErrorCode::UnknownSelection, {.field = "Index"}));
        }
        bool duplicate = false;
        for (const auto& replacement : replacements) {
            if (replacement.selection == selection) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        std::shared_ptr<IStudentIndexBase> replacement = current->createEmpty();
        replacement->build(repository);
        replacements.push_back({selection, std::move(replacement)});
    }
    for (auto& replacement : replacements) {
        for (auto& entry : indexes_) {
            if (entry.selection == replacement.selection) {
                entry.index = std::move(replacement.index);
                break;
            }
        }
    }
    return {};
}

void StudentIndexCatalog::build(const StudentRepository& repository) {
    const auto result = build(repository, getSelections());
    if (!result) {
        throw std::logic_error("invalid registered index selection");
    }
}

bool StudentIndexCatalog::hasBuiltIndexes() const noexcept {
    for (const auto& entry : indexes_) {
        if (entry.index->isBuilt()) return true;
    }
    return false;
}

bool StudentIndexCatalog::hasStaleIndexes(const StudentRepository& repository) const noexcept {
    for (const auto& entry : indexes_) {
        if (entry.index->isStale(repository)) return true;
    }
    return false;
}

StudentIndexStatistics StudentIndexCatalog::getStatistics(const StudentRepository& repository) const {
    return {repository.getCount(), getStates(repository)};
}
