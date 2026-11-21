#pragma once

#include "containers/dynamic_array.hpp"

#include <cstddef>
#include <cstdint>

using RecordId = std::uint64_t;

struct StudentRecordReference {
    std::uint64_t snapshotId{0};
    std::size_t position{0};
    RecordId recordId{0};
    friend bool operator==(const StudentRecordReference&, const StudentRecordReference&) = default;
};
using StudentRecordReferences = DynamicArray<StudentRecordReference>;
