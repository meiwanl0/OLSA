#pragma once

#include <cstdint>

#include "../container/ArchiveDictionary.h"
#include "ArchiveFacade.h"
#include "BinaryArchiveFacade.h"

namespace FTML::SmartPointer {
enum class Compatibility {
    Unresolved,
    NoExpectation,
    Exact,
    Derived,
    Different,
    UnknownExpected
};

struct ExtractResult {
    std::uint32_t expected_class_id{};
    Compatibility compatibility{Compatibility::Unresolved};
    OLSA::Container::ResolvedTypeInfo actual;
    OLSA::Container::ResolvedTypeInfo expected;

    [[nodiscard]] bool recognized() const {
        return actual.class_id != 0;
    }

    [[nodiscard]] bool matches_expected() const {
        return compatibility == Compatibility::NoExpectation
            || compatibility == Compatibility::Exact
            || compatibility == Compatibility::Derived;
    }
};

bool extract(::Archive& archive, std::uint32_t expectedClassId, ExtractResult& out);
bool extract(::BinaryArchive& archive, std::uint32_t expectedClassId, ExtractResult& out);
}  // namespace FTML::SmartPointer
