#include "../include/SmartPointer.h"
#include "../include/archive/TypeCompatibility.h"

#include <optional>

namespace {
[[nodiscard]] std::optional<OLSA::Container::ResolvedTypeInfo> resolve_expected_type(
    const ArchiveState& state,
    std::uint32_t class_id) {
    if(class_id == 0){
        return std::nullopt;
    }

    const auto class_it = state.dictionaries.classIndexById.find(class_id);
    if(class_it == state.dictionaries.classIndexById.end()){
        if(const auto recovered = OLSA::ArchiveUtil::resolve_recovered_type_info(class_id); recovered.has_value()){
            return recovered;
        }

        OLSA::Container::ResolvedTypeInfo info{};
        info.class_id = class_id;
        return info;
    }

    const auto& class_item = state.dictionaries.classList[class_it->second];
    OLSA::Container::ResolvedTypeInfo info{};
    info.class_id = class_item.class_id;
    info.class_name = class_item.name;
    info.class_version = class_item.version;
    info.module_id = class_item.module_id;
    info.known = true;

    const auto module_it = state.dictionaries.moduleIndexById.find(class_item.module_id);
    if(module_it != state.dictionaries.moduleIndexById.end()){
        const auto& module_item = state.dictionaries.moduleList[module_it->second];
        info.module_name = module_item.name;
        info.module_version = module_item.version;
    }
    return info;
}
}  // namespace

bool FTML::SmartPointer::extract(::Archive& archive, std::uint32_t expectedClassId, ExtractResult& out) {
    out = {};
    out.expected_class_id = expectedClassId;

    if(!archive.getPointer()){
        return false;
    }

    const auto& state = archive.state();
    if(!state.parsing.resolvedType.has_value()){
        return false;
    }

    out.actual = *state.parsing.resolvedType;
    if(expectedClassId == 0){
        out.compatibility = Compatibility::NoExpectation;
        return true;
    }

    const auto expected = resolve_expected_type(state, expectedClassId);
    if(expected.has_value()){
        out.expected = *expected;
    }

    out.compatibility = OLSA::ArchiveUtil::classify_type_compatibility(out.actual, out.expected);

    return true;
}

bool FTML::SmartPointer::extract(::BinaryArchive& archive, std::uint32_t expectedClassId, ExtractResult& out) {
    return extract(archive.archive(), expectedClassId, out);
}
