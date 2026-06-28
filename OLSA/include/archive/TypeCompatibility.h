#pragma once

#include <optional>

#include "../container/ArchiveDictionary.h"
#include "SmartPointerFacade.h"

namespace OLSA::ArchiveUtil {
FTML::SmartPointer::Compatibility classify_type_compatibility(
    const OLSA::Container::ResolvedTypeInfo& actual,
    const OLSA::Container::ResolvedTypeInfo& expected);
[[nodiscard]] std::optional<OLSA::Container::ResolvedTypeInfo> resolve_recovered_type_info(std::uint32_t class_id);
}  // namespace OLSA::ArchiveUtil
