#include "../include/XMSERawDataSet.h"

#include "../include/BinaryArchive.h"
#include "../include/SmartPointer.h"
#include "../include/protocol/ByteUtil.h"
#include "../include/protocol/TypeCode.h"

#include <array>
#include <cctype>
#include <cstring>
#include <functional>
#include <format>
#include <optional>
#include <string_view>

namespace FTML::XMSE::detail {
void record_field_trace(
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view item_kind,
    std::string_view name,
    std::size_t depth) {
    if(trace == nullptr){
        return;
    }
    trace->observed_fields.push_back({
        .item_kind = std::string(item_kind),
        .name = std::string(name),
        .depth = depth,
    });
}

[[nodiscard]] std::string_view current_field_name(const ItemHeader& header) {
    if(header.metaBytes.empty()){
        return {};
    }
    const auto* meta = reinterpret_cast<const char*>(header.metaBytes.data());
    return std::string_view(meta);
}

[[nodiscard]] bool expect_field(::BinaryArchive& ar, std::string_view name) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    return current_field_name(header) == name;
}

template <class T>
[[nodiscard]] bool read_scalar(::BinaryArchive& ar, TypeCode::ID type, T& out) {
    if(!ar.get(type, 1)){
        return false;
    }
    const auto& payload = ar.archive().state().parsing.itemHeader.payloadBytes;
    out = read_little_endian<T>(as_bytes(payload));
    return true;
}

[[nodiscard]] bool read_bool(::BinaryArchive& ar, bool& out) {
    std::uint8_t value{};
    if(!read_scalar(ar, TypeCode::ID::Bool, value)){
        return false;
    }
    out = value != 0;
    return true;
}

[[nodiscard]] bool is_compact_integer_type(TypeCode::ID type) {
    switch(type){
        case TypeCode::ID::Int8:
        case TypeCode::ID::UInt8:
        case TypeCode::ID::Int16:
        case TypeCode::ID::UInt16:
        case TypeCode::ID::Int32:
        case TypeCode::ID::UInt32:
            return true;
        default:
            return false;
    }
}

template <class T>
[[nodiscard]] bool read_compact_integer_traced(
    ::BinaryArchive& ar,
    TypeCode::ID request_type,
    T& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    const auto before = ar.archive().state();
    const auto header = ar.next();
    if(!header.hasItem || !is_compact_integer_type(header.signTypeCode)){
        ar.archive().state() = before;
        return false;
    }
    record_field_trace(trace, "Value", semantic_name, 0);
    return read_scalar(ar, request_type, out);
}

template <class T>
[[nodiscard]] bool read_scalar_traced(
    ::BinaryArchive& ar,
    TypeCode::ID type,
    T& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Value", semantic_name, 0);
    return read_scalar(ar, type, out);
}

[[nodiscard]] bool read_bool_traced(
    ::BinaryArchive& ar,
    bool& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Value", semantic_name, 0);
    return read_bool(ar, out);
}

[[nodiscard]] bool read_string_traced(
    ::BinaryArchive& ar,
    std::string& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Value", semantic_name, 0);
    return ar.gets(out, 0x1e0);
}

[[nodiscard]] bool is_char_payload(TypeCode::ID type);
[[nodiscard]] bool skip_current_item(::BinaryArchive& ar);

[[nodiscard]] bool read_name_string_body_until(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    std::string& out) {
    out.clear();
    while(true){
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return ar.get(end_marker, 1);
        }
        if(is_char_payload(header.payloadType)){
            std::string chunk;
            if(!ar.gets(chunk, 0x1e0)){
                return false;
            }
            out += chunk;
            continue;
        }
        if(header.signTypeCode == TypeCode::ID::BeginGroup){
            if(!ar.get(TypeCode::ID::BeginGroup, 1)){
                return false;
            }
            std::string nested;
            if(!read_name_string_body_until(ar, TypeCode::ID::EndGroup, nested)){
                return false;
            }
            out += nested;
            continue;
        }
        if(header.signTypeCode == TypeCode::ID::BeginObject){
            if(!ar.getObject()){
                return false;
            }
            std::string nested;
            if(!read_name_string_body_until(ar, TypeCode::ID::EndObject, nested)){
                return false;
            }
            out += nested;
            continue;
        }
        if(!skip_current_item(ar)){
            return false;
        }
    }
}

[[nodiscard]] bool read_textid_traced(
    ::BinaryArchive& ar,
    std::string& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    // FTMLCore::TextID::get ultimately reads a Char payload from the current archive item.
    record_field_trace(trace, "Value", semantic_name, 0);
    return ar.gets(out, 0x1e0);
}

[[nodiscard]] bool read_name_string_traced(
    ::BinaryArchive& ar,
    std::string& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    // FTML::NVT::NameString is a distinct type on the legacy side; keep a dedicated
    // entry point so its reader can diverge from plain string/TextID later.
    // FTMLUtil stores multi-part names inside the backing string with embedded NULs;
    // preserve the raw bytes here and leave display formatting to the caller.
    record_field_trace(trace, "Value", semantic_name, 0);
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    if(is_char_payload(header.payloadType)){
        return ar.gets(out, 0x1e0);
    }
    if(header.signTypeCode == TypeCode::ID::BeginGroup){
        return ar.get(TypeCode::ID::BeginGroup, 1)
            && read_name_string_body_until(ar, TypeCode::ID::EndGroup, out);
    }
    if(header.signTypeCode == TypeCode::ID::BeginObject){
        return ar.getObject()
            && read_name_string_body_until(ar, TypeCode::ID::EndObject, out);
    }
    return false;
}

[[nodiscard]] bool is_char_payload(TypeCode::ID type) {
    return type == TypeCode::ID::Char || type == TypeCode::ID::UChar;
}

[[nodiscard]] bool looks_like_name_string_header(const ItemHeader& header) {
    return is_char_payload(header.payloadType)
        || header.signTypeCode == TypeCode::ID::BeginGroup
        || header.signTypeCode == TypeCode::ID::BeginObject;
}

[[nodiscard]] bool class_name_ends_with(std::string_view class_name, std::string_view suffix) {
    return class_name.size() >= suffix.size() && class_name.ends_with(suffix);
}

[[nodiscard]] bool is_configuration_like(std::string_view class_name) {
    return class_name_ends_with(class_name, "FTML::DC::Configuration")
        || class_name_ends_with(class_name, "FTML::XMSE::Configuration");
}

[[nodiscard]] bool is_named_value_set_like(std::string_view class_name) {
    return class_name_ends_with(class_name, "FTML::NamedValueSet")
        || class_name_ends_with(class_name, "FTML::NamedValueTable");
}

[[nodiscard]] bool is_named_value_list_like(std::string_view class_name) {
    return class_name_ends_with(class_name, "FTML::NamedValueList");
}

[[nodiscard]] std::string_view item_type_name(TypeCode::ID type) {
    switch(type){
    case TypeCode::ID::Bool:
        return "bool";
    case TypeCode::ID::Char:
        return "char";
    case TypeCode::ID::UChar:
        return "uchar";
    case TypeCode::ID::Int8:
        return "int8";
    case TypeCode::ID::UInt8:
        return "uint8";
    case TypeCode::ID::Int16:
        return "int16";
    case TypeCode::ID::UInt16:
        return "uint16";
    case TypeCode::ID::Int32:
        return "int32";
    case TypeCode::ID::UInt32:
        return "uint32";
    case TypeCode::ID::Float:
        return "float";
    case TypeCode::ID::Double:
        return "double";
    case TypeCode::ID::BeginGroup:
        return "BeginGroup";
    case TypeCode::ID::BeginPointer:
        return "BeginPointer";
    case TypeCode::ID::BeginObject:
        return "BeginObject";
    case TypeCode::ID::EndGroup:
        return "EndGroup";
    case TypeCode::ID::EndPointer:
        return "EndPointer";
    case TypeCode::ID::EndObject:
        return "EndObject";
    default:
        return "item";
    }
}

[[nodiscard]] std::string summarize_value_item(const ItemHeader& header) {
    const auto bytes = as_bytes(header.payloadBytes);
    switch(header.signTypeCode){
    case TypeCode::ID::Bool:
        return std::format(
            "value=bool({})",
            read_little_endian<std::uint8_t>(bytes) != 0 ? "true" : "false");
    case TypeCode::ID::Int8:
        return std::format("value=int8({})", static_cast<int>(read_little_endian<std::int8_t>(bytes)));
    case TypeCode::ID::UInt8:
    case TypeCode::ID::UChar:
        return std::format("value=uint8({})", static_cast<unsigned>(read_little_endian<std::uint8_t>(bytes)));
    case TypeCode::ID::Int16:
        return std::format("value=int16({})", read_little_endian<std::int16_t>(bytes));
    case TypeCode::ID::UInt16:
        return std::format("value=uint16({})", read_little_endian<std::uint16_t>(bytes));
    case TypeCode::ID::Int32:
        return std::format("value=int32({})", read_little_endian<std::int32_t>(bytes));
    case TypeCode::ID::UInt32:
        return std::format("value=uint32({})", read_little_endian<std::uint32_t>(bytes));
    case TypeCode::ID::Float:
        return std::format("value=float({:.6f})", read_little_endian<float>(bytes));
    case TypeCode::ID::Double:
        return std::format("value=double({:.6f})", read_little_endian<double>(bytes));
    default:
        return std::format("value={}", item_type_name(header.signTypeCode));
    }
}

[[nodiscard]] OLSA::Container::ResolvedTypeInfo resolve_type_info_from_dictionary(
    const ArchiveState::Dictionaries& dictionaries,
    std::uint32_t class_id) {
    OLSA::Container::ResolvedTypeInfo result{};
    result.class_id = class_id;
    if(class_id == 0){
        return result;
    }
    const auto class_it = dictionaries.classIndexById.find(class_id);
    if(class_it == dictionaries.classIndexById.end()){
        return result;
    }
    const auto& class_item = dictionaries.classList[class_it->second];
    result.class_version = class_item.version;
    result.module_id = class_item.module_id;
    result.class_name = class_item.name;
    result.known = true;
    const auto module_it = dictionaries.moduleIndexById.find(class_item.module_id);
    if(module_it != dictionaries.moduleIndexById.end()){
        const auto& module_item = dictionaries.moduleList[module_it->second];
        result.module_name = module_item.name;
        result.module_version = module_item.version;
    }
    return result;
}

[[nodiscard]] bool iequals_ascii(std::string_view lhs, std::string_view rhs) {
    if(lhs.size() != rhs.size()){
        return false;
    }
    for(std::size_t i = 0; i < lhs.size(); ++i){
        const auto left = static_cast<unsigned char>(lhs[i]);
        const auto right = static_cast<unsigned char>(rhs[i]);
        if(std::tolower(left) != std::tolower(right)){
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string lookup_xmse_std_config_name(std::uint32_t value) {
    switch(value){
    case 0x0142u:
        return "XMSErprc65";
    default:
        return {};
    }
}

[[nodiscard]] bool read_hwsetup_ref_traced(
    ::BinaryArchive& ar,
    std::uint32_t& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Value", semantic_name, 0);
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    if(is_char_payload(header.payloadType)){
        std::string text;
        out = 0;
        return ar.gets(text, 0x1e0);
    }
    return read_scalar(ar, TypeCode::ID::UInt32, out);
}

[[nodiscard]] bool read_has_min_max_traced(
    ::BinaryArchive& ar,
    bool& has_min,
    bool& has_max,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Value", semantic_name, 0);
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    if(is_char_payload(header.payloadType)){
        std::string text;
        if(!ar.gets(text, 0x1e0)){
            return false;
        }
        if(iequals_ascii(text, "MinMax")){
            has_min = true;
            has_max = true;
            return true;
        }
        if(iequals_ascii(text, "Min")){
            has_min = true;
            has_max = false;
            return true;
        }
        if(iequals_ascii(text, "Max")){
            has_min = false;
            has_max = true;
            return true;
        }
        return false;
    }

    std::int32_t flags{};
    if(!read_scalar(ar, TypeCode::ID::Int32, flags)){
        return false;
    }
    has_min = (flags & 0x1) != 0;
    has_max = (flags & 0x2) != 0;
    return true;
}

[[nodiscard]] bool read_flag_value(::BinaryArchive& ar, std::string_view name, FTML::FlagValue<float>& out) {
    if(!expect_field(ar, name)){
        return false;
    }
    if(!read_bool(ar, out.has)){
        return false;
    }
    if(out.has){
        return read_scalar(ar, TypeCode::ID::Float, out.value);
    }
    out.value = 0.0f;
    return true;
}

[[nodiscard]] std::string_view known_tilt_units_name(std::uint32_t raw_units) {
    switch(raw_units){
    case 0x013148DAu:
        return "radians";
    case 0x01314924u:
        return "arcseconds";
    default:
        return {};
    }
}

struct SysInfoSemanticAnnotation {
    FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus status{
        FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus::Unclassified};
    std::string_view meaning;
    std::string_view note;
};

struct SysInfoSemanticRule {
    std::string_view fieldName;
    SysInfoSemanticAnnotation annotation;
};

using Node3SemanticStatus = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus;

[[nodiscard]] constexpr auto node3_sysinfo_semantic_rules() {
    using Annotation = SysInfoSemanticAnnotation;
    return std::to_array<SysInfoSemanticRule>({
        SysInfoSemanticRule{"LotID", {Node3SemanticStatus::FullyRestored, "lot identifier", {}}},
        SysInfoSemanticRule{"RunID", {Node3SemanticStatus::FullyRestored, "run identifier", {}}},
        SysInfoSemanticRule{"WaferSlotNumber", {Node3SemanticStatus::FullyRestored, "wafer slot number", {}}},
        SysInfoSemanticRule{"WaferOrdinalNumber", {Node3SemanticStatus::FullyRestored, "wafer ordinal within lot", {}}},
        SysInfoSemanticRule{
            "LibraryName",
            {Node3SemanticStatus::FullyRestored, "library name", "Current sample is empty; keep empty output as a real value, not a missing parse."}},
        SysInfoSemanticRule{"TestID", {Node3SemanticStatus::FullyRestored, "test identifier", {}}},
        SysInfoSemanticRule{"TestLabel", {Node3SemanticStatus::FullyRestored, "test label", {}}},
        SysInfoSemanticRule{"RecipeName", {Node3SemanticStatus::FullyRestored, "recipe name", {}}},
        SysInfoSemanticRule{"ToolSerialNumber", {Node3SemanticStatus::FullyRestored, "tool serial number", {}}},
        SysInfoSemanticRule{"SoftwareVersion", {Node3SemanticStatus::FullyRestored, "software version", {}}},
        SysInfoSemanticRule{
            "sMCB Firmware Version",
            {Node3SemanticStatus::FullyRestored, "firmware version string", "The composite text format is preserved as-is; sub-components are not split further."}},
        SysInfoSemanticRule{"FTML Version", {Node3SemanticStatus::FullyRestored, "FTML runtime version", {}}},
        SysInfoSemanticRule{"WaferSize", {Node3SemanticStatus::FullyRestored, "wafer size", {}}},
        SysInfoSemanticRule{"ToolTitle", {Node3SemanticStatus::FullyRestored, "tool title", {}}},
        SysInfoSemanticRule{"SubSystemName", {Node3SemanticStatus::FullyRestored, "subsystem name", {}}},
        SysInfoSemanticRule{"SubSystemIndex", {Node3SemanticStatus::FullyRestored, "subsystem index", {}}},
        SysInfoSemanticRule{"SiteSerialNumber", {Node3SemanticStatus::FullyRestored, "site serial number", {}}},
        SysInfoSemanticRule{"WaferID", {Node3SemanticStatus::FullyRestored, "wafer identifier", {}}},
        SysInfoSemanticRule{"Data Acquisition Start Time", {Node3SemanticStatus::FullyRestored, "acquisition start time", {}}},
        SysInfoSemanticRule{
            "Data Acquisition End Time",
            {Node3SemanticStatus::FullyRestored, "acquisition end time", "Current sample is empty; keep empty output as a real archived value."}},
        SysInfoSemanticRule{
            "FieldX",
            {Node3SemanticStatus::FullyRestored, "site/field X coordinate", "Promoted under the relaxed node[3] standard; coordinate frame is field-level, not a cell-level map."}},
        SysInfoSemanticRule{
            "FieldY",
            {Node3SemanticStatus::FullyRestored, "site/field Y coordinate", "Promoted under the relaxed node[3] standard; coordinate frame is field-level, not a cell-level map."}},

        SysInfoSemanticRule{
            "AOI",
            {Node3SemanticStatus::ClosedWithCaveat, "angle of incidence parameter", "Recovered from stable naming and sample context; the final consumer/enum chain is still not fully restored."}},
        SysInfoSemanticRule{
            "ND Filter",
            {Node3SemanticStatus::ClosedWithCaveat, "ND filter setting", "Treated as a stable process/filter parameter; exact hardware enum table is still absent."}},
        SysInfoSemanticRule{
            "UV 400 Filter",
            {Node3SemanticStatus::ClosedWithCaveat, "filter enable flag", "Recovered as a stable filter-state flag; exact hardware wiring is not expanded further."}},
        SysInfoSemanticRule{
            "UV 320 Filter",
            {Node3SemanticStatus::ClosedWithCaveat, "filter enable flag", "Recovered as a stable filter-state flag; exact hardware wiring is not expanded further."}},
        SysInfoSemanticRule{
            "UV 240 Filter",
            {Node3SemanticStatus::ClosedWithCaveat, "filter enable flag", "Recovered as a stable filter-state flag; exact hardware wiring is not expanded further."}},
        SysInfoSemanticRule{
            "Yellow Filter",
            {Node3SemanticStatus::ClosedWithCaveat, "filter enable flag", "Recovered as a stable filter-state flag; exact hardware wiring is not expanded further."}},
        SysInfoSemanticRule{
            "Alignment Mode",
            {Node3SemanticStatus::ClosedWithCaveat, "alignment mode", "Mode name is stable in the sample; the full enum definition is still missing."}},
        SysInfoSemanticRule{
            "SE Pixel Binning Mode",
            {Node3SemanticStatus::ClosedWithCaveat, "pixel binning setting", "Recovered from stable naming; deeper algorithm-specific handling is not restored yet."}},
        SysInfoSemanticRule{
            "SE Pixel Binning Size",
            {Node3SemanticStatus::ClosedWithCaveat, "pixel binning setting", "Recovered from stable naming; deeper algorithm-specific handling is not restored yet."}},
        SysInfoSemanticRule{
            "ApplyIDN",
            {Node3SemanticStatus::ClosedWithCaveat, "processing option flag", "Recovered as a stable processing/configuration switch; downstream consumer coverage is still partial."}},
        SysInfoSemanticRule{
            "ApplyDC_RPRC",
            {Node3SemanticStatus::ClosedWithCaveat, "processing option flag", "Recovered as a stable processing/configuration switch; downstream consumer coverage is still partial."}},
        SysInfoSemanticRule{
            "FMA",
            {Node3SemanticStatus::ClosedWithCaveat, "processing option flag", "Recovered as a stable processing/configuration switch; downstream consumer coverage is still partial."}},
        SysInfoSemanticRule{
            "9K_Mode",
            {Node3SemanticStatus::ClosedWithCaveat, "processing option flag", "Recovered as a stable processing/configuration switch; downstream consumer coverage is still partial."}},
        SysInfoSemanticRule{
            "RPRC Harmonics",
            {Node3SemanticStatus::ClosedWithCaveat, "RPRC processing option", "Recovered as a stable option name; final algorithm-specific interpretation remains partial."}},
        SysInfoSemanticRule{
            "RPRC Mueller Matrix",
            {Node3SemanticStatus::ClosedWithCaveat, "RPRC processing option", "Recovered as a stable option name; final algorithm-specific interpretation remains partial."}},
        SysInfoSemanticRule{
            "StaticRepeatCount",
            {Node3SemanticStatus::ClosedWithCaveat, "repeat-count parameter", "Value and role are stable; broader scheduling semantics are not expanded."}},
        SysInfoSemanticRule{
            "SiteID",
            {Node3SemanticStatus::ClosedWithCaveat, "site identifier", "Stable as archived site metadata; the internal encoding rule behind the raw integer is not expanded."}},
        SysInfoSemanticRule{
            "ToolType",
            {Node3SemanticStatus::ClosedWithCaveat, "tool type code", "The raw type code is stable, but the final enum label is not recovered."}},
        SysInfoSemanticRule{
            "SubSystemTypeAcushape",
            {Node3SemanticStatus::ClosedWithCaveat, "subsystem/module parameter", "Recovered as a stable subsystem/module field; final business naming is still approximate."}},
        SysInfoSemanticRule{
            "UVSE",
            {Node3SemanticStatus::ClosedWithCaveat, "subsystem/module parameter", "Recovered as a stable subsystem/module field; final business naming is still approximate."}},
        SysInfoSemanticRule{
            "Crossover",
            {Node3SemanticStatus::ClosedWithCaveat, "subsystem/module parameter", "Recovered as a stable subsystem/module field; final business naming is still approximate."}},
        SysInfoSemanticRule{
            "IRSE_Module",
            {Node3SemanticStatus::ClosedWithCaveat, "subsystem/module parameter", "Recovered as a stable subsystem/module field; final business naming is still approximate."}},
        SysInfoSemanticRule{
            "Cell_X",
            {Node3SemanticStatus::ClosedWithCaveat, "cell/grid coordinate", "Likely a cell/grid coordinate, but the exact wafer-map frame is still not fully restored."}},
        SysInfoSemanticRule{
            "Cell_Y",
            {Node3SemanticStatus::ClosedWithCaveat, "cell/grid coordinate", "Likely a cell/grid coordinate, but the exact wafer-map frame is still not fully restored."}},
        SysInfoSemanticRule{
            "GroupID",
            {Node3SemanticStatus::ClosedWithCaveat, "group/item metadata", "Recovered as stable grouping metadata; the owning group model is still unresolved."}},
        SysInfoSemanticRule{
            "Group Item Count",
            {Node3SemanticStatus::ClosedWithCaveat, "group/item metadata", "Recovered as stable grouping metadata; the owning group model is still unresolved."}},
        SysInfoSemanticRule{
            "Group ItemID",
            {Node3SemanticStatus::ClosedWithCaveat, "group/item metadata", "Recovered as stable grouping metadata; the owning group model is still unresolved."}},

        SysInfoSemanticRule{
            "Acquisition Order",
            {Node3SemanticStatus::Opaque, "opaque acquisition-order metadata", "Current sample preserves a stable int32 payload, but its business semantics are still unknown."}},
    });
}

[[nodiscard]] std::optional<SysInfoSemanticAnnotation> lookup_node3_sysinfo_annotation(std::string_view name) {
    // Keep the relaxed node[3] semantic policy in a single rule table so future field
    // promotions only need data updates instead of more reader branching.
    for(const auto& rule : node3_sysinfo_semantic_rules()){
        if(rule.fieldName == name){
            return rule.annotation;
        }
    }
    return std::nullopt;
}

void apply_node3_sysinfo_annotation(
    FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::StructuredScalarEntry& entry,
    bool annotate_as_node3_sysinfo) {
    if(!annotate_as_node3_sysinfo){
        return;
    }
    const auto annotation = lookup_node3_sysinfo_annotation(entry.name);
    if(!annotation.has_value()){
        return;
    }
    entry.semanticStatus = annotation->status;
    entry.semanticMeaning = std::string(annotation->meaning);
    entry.semanticNote = std::string(annotation->note);
}

[[nodiscard]] bool read_tilt_units(::BinaryArchive& ar, FTML::XMSE::RawDataSet::TiltSummary& out) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    if(is_char_payload(header.payloadType)){
        if(!ar.gets(out.unitsText, 0x1e0)){
            return false;
        }
        out.hasUnitsText = true;
        return true;
    }
    if(header.signTypeCode == TypeCode::ID::UInt8
        || header.signTypeCode == TypeCode::ID::UInt16
        || header.signTypeCode == TypeCode::ID::UInt32
        || header.signTypeCode == TypeCode::ID::Int8
        || header.signTypeCode == TypeCode::ID::Int16
        || header.signTypeCode == TypeCode::ID::Int32){
        const auto bytes = as_bytes(header.payloadBytes);
        switch(header.signTypeCode){
        case TypeCode::ID::UInt8:
        case TypeCode::ID::UChar:
            out.unitsRaw = read_little_endian<std::uint8_t>(bytes);
            break;
        case TypeCode::ID::UInt16:
            out.unitsRaw = read_little_endian<std::uint16_t>(bytes);
            break;
        case TypeCode::ID::UInt32:
            out.unitsRaw = read_little_endian<std::uint32_t>(bytes);
            break;
        case TypeCode::ID::Int8:
            out.unitsRaw = static_cast<std::uint32_t>(read_little_endian<std::uint8_t>(bytes));
            break;
        case TypeCode::ID::Int16:
            out.unitsRaw = static_cast<std::uint32_t>(read_little_endian<std::uint16_t>(bytes));
            break;
        case TypeCode::ID::Int32:
            out.unitsRaw = static_cast<std::uint32_t>(read_little_endian<std::int32_t>(bytes));
            break;
        default:
            break;
        }
        out.hasUnitsRaw = true;
        if(const auto known_name = known_tilt_units_name(out.unitsRaw); !known_name.empty()){
            out.unitsText = std::string(known_name);
            out.hasUnitsText = true;
        }
        return skip_current_item(ar);
    }
    return false;
}

[[nodiscard]] bool read_tilt_angle(::BinaryArchive& ar, float& out) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    if(header.signTypeCode == TypeCode::ID::Float){
        return read_scalar(ar, TypeCode::ID::Float, out);
    }
    if(header.signTypeCode == TypeCode::ID::Double){
        double value{};
        if(!read_scalar(ar, TypeCode::ID::Double, value)){
            return false;
        }
        out = static_cast<float>(value);
        return true;
    }
    return false;
}

[[nodiscard]] bool read_tilt_object(::BinaryArchive& ar, FTML::XMSE::RawDataSet::TiltSummary& out) {
    const auto header = ar.next();
    if(!header.hasItem || header.signTypeCode != TypeCode::ID::BeginObject){
        return false;
    }
    if(!ar.getObject()){
        return false;
    }
    std::uint32_t anonymous_index = 0;
    while(true){
        const auto item = ar.next();
        if(!item.hasItem){
            return false;
        }
        if(item.signTypeCode == TypeCode::ID::EndObject){
            return ar.get(TypeCode::ID::EndObject, 1);
        }
        const auto field_name = current_field_name(item);
        if(field_name == "Theta"){
            if(!read_tilt_angle(ar, out.theta)){
                return false;
            }
            out.hasTheta = true;
            continue;
        }
        if(field_name == "Phi"){
            if(!read_tilt_angle(ar, out.phi)){
                return false;
            }
            out.hasPhi = true;
            continue;
        }
        if(field_name == "Units"){
            if(!read_tilt_units(ar, out)){
                return false;
            }
            continue;
        }
        if(field_name.empty()){
            if(anonymous_index == 0){
                if(!read_tilt_angle(ar, out.theta)){
                    return false;
                }
                out.hasTheta = true;
                anonymous_index += 1;
                continue;
            }
            if(anonymous_index == 1){
                if(!read_tilt_angle(ar, out.phi)){
                    return false;
                }
                out.hasPhi = true;
                anonymous_index += 1;
                continue;
            }
            if(anonymous_index == 2){
                if(!read_tilt_units(ar, out)){
                    return false;
                }
                anonymous_index += 1;
                continue;
            }
        }
        if(!skip_current_item(ar)){
            return false;
        }
    }
}

[[nodiscard]] bool skip_current_item(::BinaryArchive& ar);
[[nodiscard]] bool skip_until(::BinaryArchive& ar, TypeCode::ID end_marker);
[[nodiscard]] bool skip_raw_item_header(
    ArchiveState& state,
    TypeCode::ID expected,
    std::uint32_t payload_size,
    bool expect_format);
[[nodiscard]] bool skip_raw_item_header_one_of(
    ArchiveState& state,
    std::initializer_list<TypeCode::ID> expected_types,
    std::uint32_t payload_size,
    bool expect_format);
[[nodiscard]] bool read_known_uint32_array_body(
    ::BinaryArchive& ar,
    std::uint32_t class_id,
    std::uint64_t object_id,
    TypeCode::ID end_marker,
    FTML::XMSE::RawData::UInt32ArrayPayload& out);
[[nodiscard]] bool has_exact_array_extent(const FTML::XMSE::RawData::UInt32ArrayPayload& payload);
[[nodiscard]] std::string summarize_raw_data_shape_semantics(const FTML::XMSE::RawData& value);

[[nodiscard]] const ArchiveUtil::ClassItem* find_class_item(const ArchiveState& state, std::uint32_t class_id) {
    const auto it = state.dictionaries.classIndexById.find(class_id);
    if(it == state.dictionaries.classIndexById.end()){
        return nullptr;
    }
    return &state.dictionaries.classList[it->second];
}

[[nodiscard]] bool is_known_array_pointer(const ::BinaryArchive& ar, std::uint32_t class_id, std::string_view expected_name) {
    if(expected_name == "FTML::Array2D<unsigned>" && class_id == 0x001D42B7u){
        return true;
    }
    if(expected_name == "FTML::Array<unsigned>" && class_id == 0x001D401Eu){
        return true;
    }
    if(class_id == 0){
        return false;
    }
    const auto* item = find_class_item(ar.archive().state(), class_id);
    return item != nullptr && item->name == expected_name;
}

[[nodiscard]] bool read_expected_item(::BinaryArchive& ar, TypeCode::ID expected) {
    const auto header = ar.next();
    return header.hasItem && header.signTypeCode == expected && ar.get(expected, 1);
}

[[nodiscard]] bool read_expected_item_one_of(
    ::BinaryArchive& ar,
    std::initializer_list<TypeCode::ID> expected_types) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    const auto matches = std::ranges::find(expected_types, header.signTypeCode) != expected_types.end();
    return matches && ar.get(header.signTypeCode, 1);
}

[[nodiscard]] bool read_formatted_uint32_array(::BinaryArchive& ar, std::vector<std::uint32_t>* out_values = nullptr) {
    const auto header = ar.next();
    if(!header.hasItem || header.signTypeCode != TypeCode::ID::UInt32 || header.payloadType != TypeCode::ID::UInt32){
        return false;
    }
    if(header.payloadBytes.empty() || header.payloadBytes.size() % sizeof(std::uint32_t) != 0){
        return false;
    }

    const auto count = header.payloadBytes.size() / sizeof(std::uint32_t);
    if(!ar.get(TypeCode::ID::UInt32, count)){
        return false;
    }
    if(out_values == nullptr){
        return true;
    }

    out_values->resize(count);
    std::memcpy(out_values->data(), header.payloadBytes.data(), header.payloadBytes.size());
    return true;
}

[[nodiscard]] bool read_raw_formatted_uint32_array_values(
    ArchiveState& state,
    std::vector<std::uint32_t>& out_values) {
    out_values.clear();
    state.parsing.itemHeader = {};
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto raw = state.input.buffer[state.input.offset++];
    const auto sign = static_cast<TypeCode::ID>(raw & 0x3F);
    const auto has_meta = (raw & 0x40) != 0;
    const auto is_format = (raw & 0x80) != 0;
    if(has_meta || !is_format || sign != TypeCode::ID::UInt32){
        return false;
    }
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto format_header = state.input.buffer[state.input.offset++];
    const auto packed_len_size = static_cast<std::uint8_t>(format_header >> 4);
    const auto count_size = static_cast<std::uint8_t>(format_header & 0x0F);
    if(count_size == 0 || state.input.offset + count_size + packed_len_size > state.input.buffer.size()){
        return false;
    }

    const auto count = read_little_endian<std::uint32_t>(
        std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, count_size));
    state.input.offset += count_size;
    const auto packed_len = packed_len_size == 0
        ? 0u
        : read_little_endian<std::uint32_t>(
              std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, packed_len_size));
    state.input.offset += packed_len_size;
    if(count == 0){
        return false;
    }

    const auto total_bytes = static_cast<std::size_t>(count) * sizeof(std::uint32_t);
    std::vector<std::uint8_t> payload_bytes;
    if(packed_len == 0){
        if(state.input.offset + total_bytes > state.input.buffer.size()){
            return false;
        }
        payload_bytes.assign(
            state.input.buffer.begin() + state.input.offset,
            state.input.buffer.begin() + state.input.offset + total_bytes);
        state.input.offset += static_cast<std::uint32_t>(total_bytes);
    }else{
        if(state.input.offset + packed_len > state.input.buffer.size()){
            return false;
        }
        const auto src = std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, packed_len);
        if(!state.parsing.compress.unpack(src, payload_bytes) || payload_bytes.size() != total_bytes){
            return false;
        }
        state.input.offset += packed_len;
    }

    out_values.resize(count);
    for(std::uint32_t i = 0; i < count; ++i){
        out_values[i] = read_little_endian<std::uint32_t>(
            std::span<const std::uint8_t>(payload_bytes.data() + static_cast<std::size_t>(i) * sizeof(std::uint32_t), sizeof(std::uint32_t)));
    }
    state.parsing.itemHeader = {};
    return true;
}

[[nodiscard]] bool read_byte_scalar_one_of(
    ::BinaryArchive& ar,
    std::initializer_list<TypeCode::ID> expected_types,
    std::uint32_t& out_value) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }
    const auto matches = std::ranges::find(expected_types, header.signTypeCode) != expected_types.end();
    if(!matches || !ar.get(header.signTypeCode, 1)){
        return false;
    }
    const auto& payload = ar.archive().state().parsing.itemHeader.payloadBytes;
    if(payload.size() != 1){
        return false;
    }
    out_value = payload.front();
    return true;
}

[[nodiscard]] bool read_known_uint32_array_body(
    ::BinaryArchive& ar,
    std::uint32_t class_id,
    std::uint64_t object_id,
    TypeCode::ID end_marker,
    FTML::XMSE::RawData::UInt32ArrayPayload& out) {
    out = {};
    out.class_id = class_id;
    out.object_id = object_id;

    if(is_known_array_pointer(ar, class_id, "FTML::Array2D<unsigned>")){
        out.is_2d = true;
        if(!read_byte_scalar_one_of(ar, {TypeCode::ID::Int8, TypeCode::ID::UInt8, TypeCode::ID::UChar}, out.dim0)){
            return false;
        }
        std::int16_t dim1{};
        if(!read_scalar(ar, TypeCode::ID::Int16, dim1) || dim1 < 0){
            return false;
        }
        out.dim1 = static_cast<std::uint32_t>(dim1);
        if(!read_raw_formatted_uint32_array_values(ar.archive().state(), out.values)
            || !read_expected_item(ar, end_marker)){
            return false;
        }
        return has_exact_array_extent(out);
    }

    if(is_known_array_pointer(ar, class_id, "FTML::Array<unsigned>")){
        const auto ok = read_raw_formatted_uint32_array_values(ar.archive().state(), out.values)
            && read_expected_item(ar, end_marker);
        if(ok){
            out.dim0 = static_cast<std::uint32_t>(out.values.size());
        }
        return ok;
    }

    return false;
}

[[nodiscard]] bool has_exact_array_extent(const FTML::XMSE::RawData::UInt32ArrayPayload& payload) {
    if(!payload.is_2d){
        return true;
    }
    const auto expected = static_cast<std::uint64_t>(payload.dim0) * static_cast<std::uint64_t>(payload.dim1);
    return expected == payload.values.size();
}

[[nodiscard]] std::string summarize_raw_data_shape_semantics(const FTML::XMSE::RawData& value) {
    const auto sig_rows = value.sig.has_value() ? value.sig->dim0 : 0u;
    const auto sig_cols = value.sig.has_value() ? value.sig->dim1 : 0u;
    const auto enc1_count = value.enc1.has_value() ? static_cast<std::uint32_t>(value.enc1->element_count()) : 0u;
    const auto enc2_count = value.enc2.has_value() ? static_cast<std::uint32_t>(value.enc2->element_count()) : 0u;
    const auto clk_count = value.clk.has_value() ? static_cast<std::uint32_t>(value.clk->element_count()) : 0u;
    const auto bm_count = value.bm.has_value() ? static_cast<std::uint32_t>(value.bm->element_count()) : 0u;
    const auto sig_extent_ok = !value.sig.has_value() || has_exact_array_extent(*value.sig);
    const auto base_check = sig_rows == value.numSums && sig_cols == static_cast<std::uint32_t>(value.numPixel);
    const auto bm_check = value.numBM == 0 ? bm_count == 0u : bm_count == sig_rows;
    const auto ir_tail_counts_ok = enc1_count == value.numSums
        && enc2_count == value.numSums
        && clk_count == value.numSums
        && bm_check;
    const auto single_row_cycle_compatible = value.sumsPerCycle != 0
        && sig_rows != 0
        && sig_cols == static_cast<std::uint32_t>(value.numPixel)
        && static_cast<std::uint64_t>(sig_rows) * static_cast<std::uint64_t>(value.sumsPerCycle)
            == static_cast<std::uint64_t>(value.numSums);
    return std::format(
        "shape sig={}x{} sig_elements={} sig_extent_ok={} check(false)={} expected_sig={}x{} "
        "sumsPerCycle={} single_row_cycle_compatible={} enc1={} enc2={} clk={} bm={} tail_eq_numSums={}",
        sig_rows,
        sig_cols,
        value.sig.has_value() ? value.sig->element_count() : 0u,
        sig_extent_ok ? "true" : "false",
        base_check ? "true" : "false",
        value.numSums,
        value.numPixel,
        value.sumsPerCycle,
        single_row_cycle_compatible ? "true" : "false",
        enc1_count,
        enc2_count,
        clk_count,
        bm_count,
        ir_tail_counts_ok ? "true" : "false");
}

[[nodiscard]] bool read_known_uint32_array_pointer(
    ::BinaryArchive& ar,
    FTML::XMSE::RawData::UInt32ArrayPayload& out) {
    auto& state = ar.archive().state();
    state.parsing.itemHeader = {};
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto raw = state.input.buffer[state.input.offset++];
    const auto sign = static_cast<TypeCode::ID>(raw & 0x3F);
    const auto has_meta = (raw & 0x40) != 0;
    const auto is_format = (raw & 0x80) != 0;
    if(has_meta || is_format || sign != TypeCode::ID::BeginPointer){
        return false;
    }
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto len_info = state.input.buffer[state.input.offset++];
    const auto object_len = static_cast<std::uint8_t>(len_info >> 4);
    const auto class_len = static_cast<std::uint8_t>(len_info & 0x0F);
    if(state.input.offset + class_len + object_len > state.input.buffer.size()){
        return false;
    }

    out = {};
    out.class_id = class_len == 0
        ? 0u
        : read_little_endian<std::uint32_t>(
              std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, class_len));
    state.input.offset += class_len;
    out.object_id = object_len == 0
        ? 0u
        : read_little_endian<std::uint64_t>(
              std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, object_len));
    state.input.offset += object_len;

    const auto is_array2d = is_known_array_pointer(ar, out.class_id, "FTML::Array2D<unsigned>");
    const auto is_array1d = is_known_array_pointer(ar, out.class_id, "FTML::Array<unsigned>");
    if(!is_array2d && !is_array1d){
        return false;
    }

    out.is_2d = is_array2d;
    const auto saved_state = state;
    if(skip_raw_item_header(state, TypeCode::ID::EndPointer, 0, false)){
        return true;
    }
    state = saved_state;
    return read_known_uint32_array_body(ar, out.class_id, out.object_id, TypeCode::ID::EndPointer, out);
}

[[nodiscard]] bool read_named_raw_array_field(
    ::BinaryArchive& ar,
    FTML::XMSE::RawData::UInt32ArrayPayload& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    const auto saved_state = ar.archive().state();
    if(!read_known_uint32_array_pointer(ar, out)){
        ar.archive().state() = saved_state;
        return false;
    }
    record_field_trace(trace, "Array", semantic_name, 0);
    return true;
}

[[nodiscard]] bool read_known_uint32_array_pointer_body(
    ::BinaryArchive& ar,
    std::uint32_t class_id,
    std::uint64_t object_id,
    FTML::XMSE::RawData::UInt32ArrayPayload& out) {
    const auto saved_state = ar.archive().state();
    const auto header = ar.next();
    if(!header.hasItem
        || header.signTypeCode != TypeCode::ID::BeginPointer
        || header.tempClassId != class_id
        || header.tempObjectId != object_id){
        return false;
    }
    if(!ar.getPointer()){
        return false;
    }
    const auto is_array2d = is_known_array_pointer(ar, class_id, "FTML::Array2D<unsigned>");
    const auto is_array1d = is_known_array_pointer(ar, class_id, "FTML::Array<unsigned>");
    if(!is_array2d && !is_array1d){
        ar.archive().state() = saved_state;
        return false;
    }

    out = {};
    out.class_id = class_id;
    out.object_id = object_id;
    out.is_2d = is_array2d;
    const auto state_after_pointer = ar.archive().state();
    if(read_expected_item(ar, TypeCode::ID::EndPointer)){
        return true;
    }
    ar.archive().state() = state_after_pointer;
    if(read_known_uint32_array_body(ar, class_id, object_id, TypeCode::ID::EndPointer, out)){
        return true;
    }

    ar.archive().state() = saved_state;
    return false;
}

[[nodiscard]] bool read_anonymous_raw_data_tail_arrays(
    ::BinaryArchive& ar,
    FTML::XMSE::RawData& out,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    const auto saved_state = ar.archive().state();
    auto candidate = out;

    FTML::XMSE::RawData::UInt32ArrayPayload sig{};
    if(!read_named_raw_array_field(ar, sig, trace, "Sig")){
        ar.archive().state() = saved_state;
        return true;
    }
    if(!sig.is_2d){
        ar.archive().state() = saved_state;
        return true;
    }
    candidate.sig = std::move(sig);

    FTML::XMSE::RawData::UInt32ArrayPayload enc1{};
    if(!read_named_raw_array_field(ar, enc1, trace, "Enc1")){
        ar.archive().state() = saved_state;
        return true;
    }
    if(enc1.is_2d){
        ar.archive().state() = saved_state;
        return true;
    }
    candidate.enc1 = std::move(enc1);

    FTML::XMSE::RawData::UInt32ArrayPayload enc2{};
    if(!read_named_raw_array_field(ar, enc2, trace, "Enc2")){
        ar.archive().state() = saved_state;
        return true;
    }
    if(enc2.is_2d){
        ar.archive().state() = saved_state;
        return true;
    }
    candidate.enc2 = std::move(enc2);

    FTML::XMSE::RawData::UInt32ArrayPayload clk{};
    if(!read_named_raw_array_field(ar, clk, trace, "Clk")){
        ar.archive().state() = saved_state;
        return true;
    }
    if(clk.is_2d){
        ar.archive().state() = saved_state;
        return true;
    }
    candidate.clk = std::move(clk);

    if(out.numBM != 0){
        FTML::XMSE::RawData::UInt32ArrayPayload bm{};
        if(!read_named_raw_array_field(ar, bm, trace, "BM")){
            ar.archive().state() = saved_state;
            return true;
        }
        if(bm.is_2d){
            ar.archive().state() = saved_state;
            return true;
        }
        candidate.bm = std::move(bm);
    }

    out = std::move(candidate);
    return true;
}

[[nodiscard]] bool skip_raw_item_header(
    ArchiveState& state,
    TypeCode::ID expected,
    std::uint32_t payload_size,
    bool expect_format) {
    state.parsing.itemHeader = {};
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto raw = state.input.buffer[state.input.offset++];
    const auto sign = static_cast<TypeCode::ID>(raw & 0x3F);
    const auto has_meta = (raw & 0x40) != 0;
    const auto is_format = (raw & 0x80) != 0;
    if(has_meta || sign != expected || is_format != expect_format){
        return false;
    }
    if(state.input.offset + payload_size > state.input.buffer.size()){
        return false;
    }
    state.input.offset += payload_size;
    return true;
}

[[nodiscard]] bool skip_raw_item_header_one_of(
    ArchiveState& state,
    std::initializer_list<TypeCode::ID> expected_types,
    std::uint32_t payload_size,
    bool expect_format) {
    state.parsing.itemHeader = {};
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto raw = state.input.buffer[state.input.offset++];
    const auto sign = static_cast<TypeCode::ID>(raw & 0x3F);
    const auto has_meta = (raw & 0x40) != 0;
    const auto is_format = (raw & 0x80) != 0;
    const auto matches = std::ranges::find(expected_types, sign) != expected_types.end();
    if(has_meta || !matches || is_format != expect_format){
        return false;
    }
    if(state.input.offset + payload_size > state.input.buffer.size()){
        return false;
    }
    state.input.offset += payload_size;
    return true;
}

[[nodiscard]] bool skip_raw_formatted_uint32_array(ArchiveState& state) {
    state.parsing.itemHeader = {};
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto raw = state.input.buffer[state.input.offset++];
    const auto sign = static_cast<TypeCode::ID>(raw & 0x3F);
    const auto has_meta = (raw & 0x40) != 0;
    const auto is_format = (raw & 0x80) != 0;
    if(has_meta || !is_format || sign != TypeCode::ID::UInt32){
        return false;
    }
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }

    const auto format_header = state.input.buffer[state.input.offset++];
    const auto packed_len_size = static_cast<std::uint8_t>(format_header >> 4);
    const auto count_size = static_cast<std::uint8_t>(format_header & 0x0F);
    if(count_size == 0 || state.input.offset + count_size + packed_len_size > state.input.buffer.size()){
        return false;
    }

    const auto count = read_little_endian<std::uint32_t>(
        std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, count_size));
    state.input.offset += count_size;
    const auto packed_len = packed_len_size == 0
        ? 0u
        : read_little_endian<std::uint32_t>(
              std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, packed_len_size));
    state.input.offset += packed_len_size;
    if(count == 0){
        return false;
    }

    const auto payload_size = packed_len == 0
        ? count * TypeCode::size(static_cast<std::uint8_t>(TypeCode::ID::UInt32))
        : packed_len;
    if(state.input.offset + payload_size > state.input.buffer.size()){
        return false;
    }
    state.input.offset += payload_size;
    state.parsing.itemHeader = {};
    return true;
}

[[nodiscard]] bool skip_known_array_pointer_body(::BinaryArchive& ar, std::uint32_t class_id, std::uint64_t object_id) {
    if(object_id == 0){
        return false;
    }
    if(is_known_array_pointer(ar, class_id, "FTML::Array2D<unsigned>")){
        return read_expected_item_one_of(ar, {TypeCode::ID::Int8, TypeCode::ID::UInt8, TypeCode::ID::UChar})
            && read_expected_item(ar, TypeCode::ID::Int16)
            && read_formatted_uint32_array(ar)
            && read_expected_item(ar, TypeCode::ID::EndPointer);
    }
    if(is_known_array_pointer(ar, class_id, "FTML::Array<unsigned>")){
        return read_formatted_uint32_array(ar)
            && read_expected_item(ar, TypeCode::ID::EndPointer);
    }
    return false;
}

[[nodiscard]] bool skip_until(::BinaryArchive& ar, TypeCode::ID end_marker) {
    while(true){
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return ar.get(end_marker, 1);
        }
        if(!skip_current_item(ar)){
            return false;
        }
    }
}

[[nodiscard]] bool skip_current_item(::BinaryArchive& ar) {
    const auto header = ar.next();
    if(!header.hasItem){
        return false;
    }

    switch(header.signTypeCode){
    case TypeCode::ID::BeginGroup:
        return ar.get(TypeCode::ID::BeginGroup, 1) && skip_until(ar, TypeCode::ID::EndGroup);
    case TypeCode::ID::BeginPointer:
        if(!ar.get(TypeCode::ID::BeginPointer, 1)){
            return false;
        }
        if(skip_known_array_pointer_body(ar, header.tempClassId, header.tempObjectId)){
            return true;
        }
        if(header.tempObjectId != 0){
            const auto next_header = ar.next();
            if(!next_header.hasItem || next_header.signTypeCode == TypeCode::ID::BeginPointer){
                return true;
            }
        }
        return skip_until(ar, TypeCode::ID::EndPointer);
    case TypeCode::ID::BeginObject:
        return ar.get(TypeCode::ID::BeginObject, 1) && skip_until(ar, TypeCode::ID::EndObject);
    case TypeCode::ID::EndGroup:
        return ar.get(TypeCode::ID::EndGroup, 1);
    case TypeCode::ID::EndPointer:
        return ar.get(TypeCode::ID::EndPointer, 1);
    case TypeCode::ID::EndObject:
        return ar.get(TypeCode::ID::EndObject, 1);
    default:
        return ar.doGet(TypeCode::ID::None);
    }
}

template <class Handler>
[[nodiscard]] bool read_fields_until(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    Handler&& handler,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::size_t depth = 0) {
    while(true){
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return ar.get(end_marker, 1);
        }
        if(header.signTypeCode == TypeCode::ID::BeginGroup){
            record_field_trace(trace, "BeginGroup", current_field_name(header), depth);
            if(!ar.get(TypeCode::ID::BeginGroup, 1)
                || !read_fields_until(ar, TypeCode::ID::EndGroup, handler, trace, depth + 1)){
                return false;
            }
            continue;
        }
        if(header.signTypeCode == TypeCode::ID::BeginObject){
            record_field_trace(trace, "BeginObject", current_field_name(header), depth);
            if(!ar.getObject()
                || !read_fields_until(ar, TypeCode::ID::EndObject, handler, trace, depth + 1)){
                return false;
            }
            continue;
        }

        record_field_trace(trace, "Item", current_field_name(header), depth);
        const auto decision = handler(current_field_name(header), header);
        if(!decision.has_value()){
            if(!skip_current_item(ar)){
                return false;
            }
            continue;
        }
        if(!decision.value()){
            return false;
        }
    }
}

[[nodiscard]] bool body_uses_named_fields(::BinaryArchive& ar, TypeCode::ID end_marker) {
    const auto header = ar.next();
    if(!header.hasItem || header.signTypeCode == end_marker){
        return false;
    }
    return !current_field_name(header).empty();
}

template <class T, class TryParse>
[[nodiscard]] bool scan_anonymous_payload(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    T& out,
    bool& materialized,
    FTML::XMSE::DynamicObjectReadResult* trace,
    TryParse&& try_parse) {
    materialized = false;
    while(true){
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return ar.get(end_marker, 1);
        }

        const auto saved_state = ar.archive().state();
        T candidate{};
        FTML::XMSE::DynamicObjectReadResult probe_trace{};
        if(try_parse(ar, candidate, trace != nullptr ? &probe_trace : nullptr)){
            out = std::move(candidate);
            materialized = true;
            if(trace != nullptr){
                trace->observed_fields.insert(
                    trace->observed_fields.end(),
                    probe_trace.observed_fields.begin(),
                    probe_trace.observed_fields.end());
            }
            return skip_until(ar, end_marker);
        }

        ar.archive().state() = saved_state;
        record_field_trace(trace, "Skip", current_field_name(header), 0);
        if(!skip_current_item(ar)){
            return false;
        }
    }
}

[[nodiscard]] bool try_read_dc_recipe_anonymous(
    ::BinaryArchive& ar,
    FTML::XMSE::DCRecipe& out,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    return read_scalar_traced(ar, TypeCode::ID::UInt32, out.numCyclesRef, trace, "NumCyclesRef")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.numCyclesDark, trace, "NumCyclesDark")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.numCyclesSample, trace, "NumCyclesSample")
        && read_bool_traced(ar, out.sumCycles, trace, "SumCycles")
        && read_bool_traced(ar, out.wRangeHasMin, trace, "WRange.hasMin")
        && read_bool_traced(ar, out.wRangeHasMax, trace, "WRange.hasMax")
        && (!out.wRangeHasMin || read_scalar_traced(ar, TypeCode::ID::Float, out.wRangeMin, trace, "WRange.min"))
        && (!out.wRangeHasMax || read_scalar_traced(ar, TypeCode::ID::Float, out.wRangeMax, trace, "WRange.max"))
        && read_scalar_traced(ar, TypeCode::ID::Float, out.analyzerRef, trace, "AnalyzerRef")
        && read_scalar_traced(ar, TypeCode::ID::Float, out.analyzerSample, trace, "AnalyzerSample")
        && read_scalar_traced(ar, TypeCode::ID::Float, out.rotation, trace, "Rotation")
        && read_bool_traced(ar, out.symThreshHas, trace, "SymThresh.has")
        && (!out.symThreshHas || read_scalar_traced(ar, TypeCode::ID::Float, out.symThreshValue, trace, "SymThresh.value"))
        && read_bool_traced(ar, out.sumsPerCycleHas, trace, "SumsPerCycle.has")
        && (!out.sumsPerCycleHas || read_scalar_traced(ar, TypeCode::ID::UInt32, out.sumsPerCycle, trace, "SumsPerCycle.value"))
        && read_bool_traced(ar, out.timingModeHas, trace, "TimingMode.has")
        && (!out.timingModeHas || read_scalar_traced(ar, TypeCode::ID::UInt32, out.timingMode, trace, "TimingMode.value"))
        && read_bool_traced(ar, out.saturationHas, trace, "Saturation.has")
        && (!out.saturationHas || read_scalar_traced(ar, TypeCode::ID::UInt32, out.saturation, trace, "Saturation.value"));
}

[[nodiscard]] bool try_read_dp_recipe_anonymous(
    ::BinaryArchive& ar,
    FTML::XMSE::DPRecipe& out,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    const auto before_config_app = ar.archive().state();
    const auto maybe_config_app = ar.next();
    if(!maybe_config_app.hasItem){
        return false;
    }
    if(looks_like_name_string_header(maybe_config_app)){
        if(!read_name_string_traced(ar, out.configApp, trace, "ConfigApp")){
            return false;
        }
    } else {
        ar.archive().state() = before_config_app;
    }

    const auto before_auto_results = ar.archive().state();
    const auto maybe_auto_results = ar.next();
    if(!maybe_auto_results.hasItem){
        return false;
    }
    if(maybe_auto_results.signTypeCode == TypeCode::ID::BeginPointer){
        record_field_trace(trace, "Pointer", "AutoResults", 0);
        if(!skip_current_item(ar)){
            return false;
        }
    } else {
        ar.archive().state() = before_auto_results;
    }

    return read_scalar_traced(ar, TypeCode::ID::Int32, out.binning, trace, "Binning")
        && read_bool_traced(ar, out.applyMultiScanErr, trace, "ApplyMultiScanErr")
        && read_bool_traced(ar, out.applyPSF, trace, "ApplyPSF")
        && read_bool_traced(ar, out.applyLinearity, trace, "ApplyLinearity")
        && read_bool_traced(ar, out.applyDCOffset, trace, "ApplyDCOffset")
        && read_bool_traced(ar, out.applyA0P0Offset, trace, "ApplyA0P0Offset")
        && read_bool_traced(ar, out.applyWShift, trace, "ApplyWShift")
        && read_bool_traced(ar, out.applyTilt, trace, "ApplyTilt")
        && read_bool_traced(ar, out.applyIDN, trace, "ApplyIDN")
        && read_bool_traced(ar, out.modelTilt, trace, "ModelTilt");
}

[[nodiscard]] bool try_read_raw_data_anonymous(
    ::BinaryArchive& ar,
    FTML::XMSE::RawData& out,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    std::uint32_t hw_setup{};
    std::uint16_t timestamp_year{};
    std::uint8_t timestamp_month{};
    std::uint8_t timestamp_day{};
    std::uint8_t timestamp_hour{};
    std::uint8_t timestamp_minute{};
    std::uint16_t timestamp_millisecond{};

    return read_hwsetup_ref_traced(ar, hw_setup, trace, "HWSetup")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.numSums, trace, "NumSums")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.sumsPerCycle, trace, "SumsPerCycle")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.timingMode, trace, "TimingMode")
        && read_scalar_traced(ar, TypeCode::ID::Int32, out.numPixel, trace, "NumPixel")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.turnsPerCycle0, trace, "TurnsPerCycle[0]")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.turnsPerCycle1, trace, "TurnsPerCycle[1]")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.numBM, trace, "NumBM")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.firstSum, trace, "FirstSum")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.firstAcqSum, trace, "FirstAcqSum")
        && read_scalar_traced(ar, TypeCode::ID::UInt16, timestamp_year, trace, "TimeStamp.year")
        && read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_month, trace, "TimeStamp.month")
        && read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_day, trace, "TimeStamp.day")
        && read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_hour, trace, "TimeStamp.hour")
        && read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_minute, trace, "TimeStamp.minute")
        && read_scalar_traced(ar, TypeCode::ID::UInt16, timestamp_millisecond, trace, "TimeStamp.millisecond")
        && read_has_min_max_traced(ar, out.pixelRangeHasMin, out.pixelRangeHasMax, trace, "PixelRange.flags")
        && (!out.pixelRangeHasMin || read_scalar_traced(ar, TypeCode::ID::Int32, out.pixelRangeMin, trace, "PixelRange.min"))
        && (!out.pixelRangeHasMax || read_scalar_traced(ar, TypeCode::ID::Int32, out.pixelRangeMax, trace, "PixelRange.max"))
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.clkPeriod, trace, "ClkPeriod")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.enc1Lines, trace, "Enc1Lines")
        && read_scalar_traced(ar, TypeCode::ID::UInt32, out.enc2Lines, trace, "Enc2Lines")
        && read_anonymous_raw_data_tail_arrays(ar, out, trace);
}

[[nodiscard]] bool read_configuration_pointer_entry(
    ::BinaryArchive& ar,
    std::uint32_t expected_class_id,
    FTML::XMSE::ConfigurationSetEntry& out,
    FTML::XMSE::DynamicObjectReadResult* trace,
    std::string_view semantic_name) {
    record_field_trace(trace, "Pointer", semantic_name, 0);
    const auto header = ar.next();
    if(!header.hasItem || header.signTypeCode != TypeCode::ID::BeginPointer){
        return false;
    }

    out.object_id = header.tempObjectId;
    out.tag = header.hasTag ? header.tag : 0;

    FTML::SmartPointer::ExtractResult extracted{};
    if(!FTML::SmartPointer::extract(ar, expected_class_id, extracted) || !extracted.recognized()){
        return false;
    }

    out.type = extracted.actual;
    out.expected = extracted.expected;
    out.compatibility = extracted.compatibility;
    out.body_offset = ar.archive().state().input.offset;
    FTML::XMSE::DynamicObjectReadResult config_trace{};
    auto* const cfg_trace = &config_trace;

    const auto assign_config_info = [&](FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary& summary,
                                        FTML::XMSE::ConfigurationSetEntry& config_info) {
        summary.hasConfigInfo = true;
        summary.configInfoType = config_info.type;
        summary.configInfoExpected = config_info.expected;
        summary.configInfoCompatibility = config_info.compatibility;
        summary.configInfoBodyOffset = config_info.body_offset;
        summary.configInfoObjectId = config_info.object_id;
        summary.configInfoTag = config_info.tag;
        if(config_info.namedValueSet.has_value()){
            FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary::ConfigInfoSummary config_info_summary{};
            for(const auto& nested : config_info.namedValueSet->entries){
                FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary::ConfigInfoEntrySummary entry{};
                entry.key = nested.key;
                entry.ordinal = nested.ordinal;
                entry.type = nested.type;
                entry.expected = nested.expected;
                entry.compatibility = nested.compatibility;
                entry.body_offset = nested.body_offset;
                entry.object_id = nested.object_id;
                entry.tag = nested.tag;
                config_info_summary.entries.push_back(std::move(entry));
            }
            summary.configInfoSummary = std::move(config_info_summary);
        }
    };
    const auto finalize_configuration_trace = [&](FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary& summary) {
        for(const auto& item : config_trace.observed_fields){
            summary.observedFields.push_back(std::format(
                "{}:{}",
                item.item_kind,
                item.name.empty() ? "<anonymous>" : item.name));
        }
        if(trace != nullptr){
            trace->observed_fields.insert(
                trace->observed_fields.end(),
                config_trace.observed_fields.begin(),
                config_trace.observed_fields.end());
        }
    };
    const auto try_consume_dc_configuration_tail_anonymous =
        [&](FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary& summary) -> bool {
            const auto before_tail = ar.archive().state();
            std::uint16_t extracted{};
            if(!read_compact_integer_traced(
                   ar,
                   TypeCode::ID::UInt16,
                   extracted,
                   cfg_trace,
                   "Configuration.Extracted")){
                ar.archive().state() = before_tail;
                return true;
            }

            const auto before_calibrations = ar.archive().state();
            const auto maybe_group = ar.next();
            if(!maybe_group.hasItem){
                return false;
            }
            if(maybe_group.signTypeCode != TypeCode::ID::BeginGroup){
                // A lone compact integer here is ambiguous with the first XMSE tail field.
                // Only commit the DC trailer once the following Calibrations group is present.
                ar.archive().state() = before_tail;
                return true;
            }

            FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary::DCTrailerSummary dc_trailer{};
            dc_trailer.hasExtracted = true;
            dc_trailer.extracted = extracted;
            dc_trailer.extractedStdConfigName = lookup_xmse_std_config_name(extracted);
            dc_trailer.hasCalibrationsGroup = true;
            record_field_trace(cfg_trace, "BeginGroup", "Configuration.Calibrations", 0);
            if(!ar.get(TypeCode::ID::BeginGroup, 1)){
                return false;
            }

            const auto before_entries = ar.archive().state();
            const auto try_read_entries =
                [&]() -> bool {
                    while(true){
                        const auto next_entry = ar.next();
                        if(!next_entry.hasItem){
                            return false;
                        }
                        if(next_entry.signTypeCode == TypeCode::ID::EndGroup){
                            return ar.get(TypeCode::ID::EndGroup, 1);
                        }

                        FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary::DCCalibrationEntrySummary entry{};
                        if(!read_compact_integer_traced(
                               ar,
                               TypeCode::ID::UInt32,
                               entry.settingsRaw,
                               cfg_trace,
                               "Configuration.Settings")){
                            return false;
                        }
                        entry.hasSettingsRaw = true;
                        entry.settingsStdConfigName = lookup_xmse_std_config_name(entry.settingsRaw);

                        record_field_trace(cfg_trace, "Pointer", "Configuration.Calibration", 0);
                        const auto pointer_header = ar.next();
                        if(!pointer_header.hasItem || pointer_header.signTypeCode != TypeCode::ID::BeginPointer){
                            return false;
                        }
                        entry.object_id = pointer_header.tempObjectId;
                        entry.tag = pointer_header.hasTag ? pointer_header.tag : 0;

                        FTML::SmartPointer::ExtractResult extracted_pointer{};
                        if(!FTML::SmartPointer::extract(ar, 0x49087FA9u, extracted_pointer)
                            || !extracted_pointer.recognized()){
                            return false;
                        }
                        entry.type = extracted_pointer.actual;
                        entry.expected = extracted_pointer.expected;
                        entry.compatibility = extracted_pointer.compatibility;
                        entry.body_offset = ar.archive().state().input.offset;
                        if(!skip_until(ar, TypeCode::ID::EndPointer)){
                            return false;
                        }
                        dc_trailer.calibrationEntries.push_back(std::move(entry));
                    }
                };

            if(!try_read_entries()){
                dc_trailer.calibrationEntries.clear();
                ar.archive().state() = before_entries;
                if(!skip_until(ar, TypeCode::ID::EndGroup)){
                    return false;
                }
            }

            summary.dcTrailerSummary = std::move(dc_trailer);
            return true;
        };

    const auto try_read_xmse_configuration_tail_anonymous =
        [&](FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary& summary) -> bool {
            const auto before_tail = ar.archive().state();
            const auto next = ar.next();
            if(!next.hasItem){
                return false;
            }
            if(!is_compact_integer_type(next.signTypeCode)){
                ar.archive().state() = before_tail;
                return true;
            }

            std::uint32_t sums_per_cycle{};
            std::uint32_t timing_mode{};
            std::uint32_t saturation{};
            std::uint32_t turns_per_cycle0{};
            std::uint32_t turns_per_cycle1{};
            std::uint32_t num_pixel{};
            const auto read_turns_per_cycle = [&]() -> bool {
                const auto before_turns = ar.archive().state();
                const auto turns_header = ar.next();
                if(!turns_header.hasItem){
                    return false;
                }
                if(turns_header.signTypeCode == TypeCode::ID::BeginGroup){
                    return ar.get(TypeCode::ID::BeginGroup, 1)
                        && read_scalar_traced(
                            ar,
                            TypeCode::ID::UInt32,
                            turns_per_cycle0,
                            cfg_trace,
                            "Configuration.TurnsPerCycle[0]")
                        && read_scalar_traced(
                            ar,
                            TypeCode::ID::UInt32,
                            turns_per_cycle1,
                            cfg_trace,
                            "Configuration.TurnsPerCycle[1]")
                        && ar.get(TypeCode::ID::EndGroup, 1);
                }
                ar.archive().state() = before_turns;
                return read_compact_integer_traced(
                           ar,
                           TypeCode::ID::UInt32,
                           turns_per_cycle0,
                           cfg_trace,
                           "Configuration.TurnsPerCycle[0]")
                    && read_compact_integer_traced(
                           ar,
                           TypeCode::ID::UInt32,
                           turns_per_cycle1,
                           cfg_trace,
                           "Configuration.TurnsPerCycle[1]");
            };
            if(!read_compact_integer_traced(
                   ar,
                   TypeCode::ID::UInt32,
                   sums_per_cycle,
                   cfg_trace,
                   "Configuration.SumsPerCycle")
                || !read_compact_integer_traced(
                    ar,
                    TypeCode::ID::UInt32,
                    timing_mode,
                    cfg_trace,
                    "Configuration.TimingMode")
                || !read_compact_integer_traced(
                    ar,
                    TypeCode::ID::UInt32,
                    saturation,
                    cfg_trace,
                    "Configuration.Saturation")
                || !read_turns_per_cycle()
                || !read_compact_integer_traced(
                    ar,
                    TypeCode::ID::UInt32,
                    num_pixel,
                    cfg_trace,
                    "Configuration.NumPixel")){
                ar.archive().state() = before_tail;
                return true;
            }

            summary.hasXMSETail = true;
            summary.sumsPerCycle = sums_per_cycle;
            summary.timingMode = timing_mode;
            summary.saturation = saturation;
            summary.turnsPerCycle0 = turns_per_cycle0;
            summary.turnsPerCycle1 = turns_per_cycle1;
            summary.numPixel = num_pixel;
            return true;
        };

    auto try_read_dc_configuration_summary_named = [&]() -> bool {
        FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary summary{};
        std::size_t matched_fields = 0;

        const bool ok = read_fields_until(
            ar,
            TypeCode::ID::EndPointer,
            [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
                if(name == "System"){
                    matched_fields += 1;
                    return read_textid_traced(ar, summary.system, cfg_trace, "Configuration.System");
                }
                if(name == "Comment"){
                    matched_fields += 1;
                    return read_string_traced(ar, summary.comment, cfg_trace, "Configuration.Comment");
                }
                if(name == "BaseName"){
                    matched_fields += 1;
                    return read_name_string_traced(ar, summary.baseName, cfg_trace, "Configuration.BaseName");
                }
                if(name == "ConfigInfo"){
                    FTML::XMSE::ConfigurationSetEntry config_info{};
                    matched_fields += 1;
                    if(!read_configuration_pointer_entry(ar, 0x499602DDu, config_info, cfg_trace, "Configuration.ConfigInfo")){
                        return false;
                    }
                    assign_config_info(summary, config_info);
                    return true;
                }
                if(name == "SumsPerCycle"){
                    matched_fields += 1;
                    summary.hasXMSETail = true;
                    return read_scalar_traced(
                        ar,
                        TypeCode::ID::UInt32,
                        summary.sumsPerCycle,
                        cfg_trace,
                        "Configuration.SumsPerCycle");
                }
                if(name == "TimingMode"){
                    matched_fields += 1;
                    summary.hasXMSETail = true;
                    return read_scalar_traced(
                        ar,
                        TypeCode::ID::UInt32,
                        summary.timingMode,
                        cfg_trace,
                        "Configuration.TimingMode");
                }
                if(name == "Saturation"){
                    matched_fields += 1;
                    summary.hasXMSETail = true;
                    return read_scalar_traced(
                        ar,
                        TypeCode::ID::UInt32,
                        summary.saturation,
                        cfg_trace,
                        "Configuration.Saturation");
                }
                if(name == "TurnsPerCycle"){
                    matched_fields += 1;
                    summary.hasXMSETail = true;
                    if(header.signTypeCode == TypeCode::ID::BeginGroup){
                        return ar.get(TypeCode::ID::BeginGroup, 1)
                            && read_scalar_traced(
                                ar,
                                TypeCode::ID::UInt32,
                                summary.turnsPerCycle0,
                                cfg_trace,
                                "Configuration.TurnsPerCycle[0]")
                            && read_scalar_traced(
                                ar,
                                TypeCode::ID::UInt32,
                                summary.turnsPerCycle1,
                                cfg_trace,
                                "Configuration.TurnsPerCycle[1]")
                            && ar.get(TypeCode::ID::EndGroup, 1);
                    }
                    return read_scalar_traced(
                               ar,
                               TypeCode::ID::UInt32,
                               summary.turnsPerCycle0,
                               cfg_trace,
                               "Configuration.TurnsPerCycle[0]")
                        && read_scalar_traced(
                               ar,
                               TypeCode::ID::UInt32,
                               summary.turnsPerCycle1,
                               cfg_trace,
                               "Configuration.TurnsPerCycle[1]");
                }
                if(name == "NumPixel"){
                    matched_fields += 1;
                    summary.hasXMSETail = true;
                    return read_scalar_traced(
                        ar,
                        TypeCode::ID::UInt32,
                        summary.numPixel,
                        cfg_trace,
                        "Configuration.NumPixel");
                }
                if(!name.empty() && (header.payloadType == TypeCode::ID::Char || header.payloadType == TypeCode::ID::UChar)){
                    // Some FTML string-like subobjects are still emitted as named char payloads.
                    std::string ignored;
                    return ar.gets(ignored, 0x1e0);
                }
                return std::nullopt;
            },
            cfg_trace);

        if(!ok){
            return false;
        }
        if(matched_fields != 0){
            finalize_configuration_trace(summary);
            out.configuration = std::move(summary);
        }
        return true;
    };

    auto try_read_dc_configuration_summary_anonymous = [&]() -> bool {
        FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary summary{};
        std::uint16_t timestamp_year{};
        std::uint8_t timestamp_month{};
        std::uint8_t timestamp_day{};
        std::uint8_t timestamp_hour{};
        std::uint8_t timestamp_minute{};
        std::uint16_t timestamp_sub_minute_raw{};

        if(!read_textid_traced(ar, summary.system, cfg_trace, "Configuration.System")){
            return false;
        }
        if(!read_string_traced(ar, summary.comment, cfg_trace, "Configuration.Comment")){
            return false;
        }
        if(!read_scalar_traced(ar, TypeCode::ID::UInt16, timestamp_year, cfg_trace, "Configuration.TimeStamp.year")
            || !read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_month, cfg_trace, "Configuration.TimeStamp.month")
            || !read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_day, cfg_trace, "Configuration.TimeStamp.day")
            || !read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_hour, cfg_trace, "Configuration.TimeStamp.hour")
            || !read_scalar_traced(ar, TypeCode::ID::UChar, timestamp_minute, cfg_trace, "Configuration.TimeStamp.minute")
            || !read_scalar_traced(ar, TypeCode::ID::UInt16, timestamp_sub_minute_raw, cfg_trace, "Configuration.TimeStamp.subMinuteRaw")){
            return false;
        }
        summary.hasTimestamp = true;
        summary.timestampYear = timestamp_year;
        summary.timestampMonth = timestamp_month;
        summary.timestampDay = timestamp_day;
        summary.timestampHour = timestamp_hour;
        summary.timestampMinute = timestamp_minute;
        summary.timestampSubMinuteRaw = timestamp_sub_minute_raw;

        const auto before_base_name = ar.archive().state();
        const auto maybe_base_name = ar.next();
        if(!maybe_base_name.hasItem){
            return false;
        }
        if(looks_like_name_string_header(maybe_base_name)){
            if(!read_name_string_traced(ar, summary.baseName, cfg_trace, "Configuration.BaseName")){
                return false;
            }
        } else {
            ar.archive().state() = before_base_name;
        }

        const auto before_config_info = ar.archive().state();
        const auto next = ar.next();
        if(!next.hasItem){
            return false;
        }
        if(next.signTypeCode == TypeCode::ID::BeginPointer){
            FTML::XMSE::ConfigurationSetEntry config_info{};
            if(!read_configuration_pointer_entry(ar, 0x499602DDu, config_info, cfg_trace, "Configuration.ConfigInfo")){
                return false;
            }
            assign_config_info(summary, config_info);
        } else {
            ar.archive().state() = before_config_info;
        }

        if(!try_consume_dc_configuration_tail_anonymous(summary)){
            return false;
        }

        if(!try_read_xmse_configuration_tail_anonymous(summary)){
            return false;
        }

        finalize_configuration_trace(summary);
        out.configuration = std::move(summary);
        return skip_until(ar, TypeCode::ID::EndPointer);
    };

    auto try_read_named_value_set_summary = [&]() -> bool {
        FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary summary{};
        constexpr std::size_t max_preview = 8;
        std::optional<std::string> pending_entry_name;
        std::uint32_t ordinal = 0;
        const auto append_preview = [&](std::string text) {
            if(summary.preview.size() < max_preview){
                summary.preview.push_back(std::move(text));
            }
        };

        const auto append_entry = [&](std::string_view field_name, FTML::XMSE::ConfigurationSetEntry&& nested) {
            FTML::XMSE::ConfigurationSetEntry::NamedValueSetEntrySummary entry{};
            if(!field_name.empty()){
                entry.key = std::string(field_name);
            } else if(pending_entry_name.has_value()){
                entry.key = std::move(*pending_entry_name);
                pending_entry_name.reset();
            }
            entry.ordinal = ordinal++;
            entry.type = std::move(nested.type);
            entry.expected = std::move(nested.expected);
            entry.compatibility = nested.compatibility;
            entry.body_offset = nested.body_offset;
            entry.object_id = nested.object_id;
            entry.tag = nested.tag;
            if(nested.configuration.has_value()){
                entry.configuration = std::move(nested.configuration);
            }
            if(nested.namedValueList.has_value()){
                entry.namedValueList = std::move(nested.namedValueList);
            }
            summary.itemCount += 1;
            summary.entries.push_back(std::move(entry));
        };
        const auto before_named_parse = ar.archive().state();
        struct ParsedNamedValueSetItem {
            std::string preview;
            std::string valueType;
            std::optional<std::string> text;
            std::optional<std::int32_t> signedValue;
            std::optional<std::uint32_t> unsignedValue;
            std::vector<ParsedNamedValueSetItem> groupItems;
        };
        std::vector<ParsedNamedValueSetItem> flat_scalar_items;
        const auto read_signed_value = [&](const ItemHeader& header) -> std::optional<std::int32_t> {
            const auto bytes = as_bytes(header.payloadBytes);
            switch(header.signTypeCode){
            case TypeCode::ID::Int8:
                return static_cast<std::int32_t>(read_little_endian<std::int8_t>(bytes));
            case TypeCode::ID::UInt8:
            case TypeCode::ID::UChar:
                return static_cast<std::int32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::Int16:
                return static_cast<std::int32_t>(read_little_endian<std::int16_t>(bytes));
            case TypeCode::ID::UInt16:
                return static_cast<std::int32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::Int32:
                return read_little_endian<std::int32_t>(bytes);
            case TypeCode::ID::UInt32:
                return static_cast<std::int32_t>(read_little_endian<std::uint32_t>(bytes));
            default:
                return std::nullopt;
            }
        };
        const auto read_unsigned_value = [&](const ItemHeader& header) -> std::optional<std::uint32_t> {
            const auto bytes = as_bytes(header.payloadBytes);
            switch(header.signTypeCode){
            case TypeCode::ID::UInt8:
            case TypeCode::ID::UChar:
                return static_cast<std::uint32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::UInt16:
                return static_cast<std::uint32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::UInt32:
                return read_little_endian<std::uint32_t>(bytes);
            case TypeCode::ID::Int8:
                return static_cast<std::uint32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::Int16:
                return static_cast<std::uint32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::Int32:
                return static_cast<std::uint32_t>(read_little_endian<std::int32_t>(bytes));
            default:
                return std::nullopt;
            }
        };
        const auto join_preview = [](const std::vector<std::string>& items) {
            std::string joined;
            for(std::size_t i = 0; i < items.size(); ++i){
                if(!joined.empty()){
                    joined += ", ";
                }
                joined += items[i];
            }
            return joined;
        };
        std::function<std::optional<ParsedNamedValueSetItem>()> summarize_current_item;
        summarize_current_item = [&]() -> std::optional<ParsedNamedValueSetItem> {
            const auto header = ar.next();
            if(!header.hasItem){
                return std::nullopt;
            }

            if(is_char_payload(header.payloadType)){
                std::string text;
                if(!ar.gets(text, 0x1e0)){
                    return std::nullopt;
                }
                ParsedNamedValueSetItem item{};
                item.valueType = "text";
                item.text = text;
                item.preview = std::format("text={}", text.empty() ? "<empty>" : text);
                return item;
            }

            if(header.signTypeCode == TypeCode::ID::BeginGroup){
                if(!ar.get(TypeCode::ID::BeginGroup, 1)){
                    return std::nullopt;
                }
                ParsedNamedValueSetItem item{};
                std::vector<std::string> nested_preview;
                while(true){
                    const auto before_nested = ar.archive().state();
                    const auto nested_header = ar.next();
                    if(!nested_header.hasItem){
                        return std::nullopt;
                    }
                    if(nested_header.signTypeCode == TypeCode::ID::EndGroup){
                        if(!ar.get(TypeCode::ID::EndGroup, 1)){
                            return std::nullopt;
                        }
                        break;
                    }
                    ar.archive().state() = before_nested;
                    auto nested = summarize_current_item();
                    if(!nested.has_value()){
                        return std::nullopt;
                    }
                    if(nested_preview.size() < max_preview){
                        nested_preview.push_back(nested->preview);
                    }
                    item.groupItems.push_back(std::move(*nested));
                }
                item.preview = std::format(
                    "group[{}]=[{}]",
                    item.groupItems.size(),
                    nested_preview.empty() ? "(none)" : join_preview(nested_preview));
                return item;
            }

            ParsedNamedValueSetItem item{};
            item.valueType = std::string(item_type_name(header.signTypeCode));
            item.preview = summarize_value_item(header);
            item.signedValue = read_signed_value(header);
            item.unsignedValue = read_unsigned_value(header);
            if(!skip_current_item(ar)){
                return std::nullopt;
            }
            return item;
        };
        const bool annotate_as_node3_sysinfo = semantic_name == "RawDataSet.SysInfo";
        const auto append_structured_scalars = [&](const ParsedNamedValueSetItem& item) {
            if(item.groupItems.size() < 3){
                return;
            }
            for(std::size_t i = 0; i + 2 < item.groupItems.size(); i += 3){
                const auto& name = item.groupItems[i];
                const auto& tag = item.groupItems[i + 1];
                const auto& value = item.groupItems[i + 2];
                if(!name.text.has_value() || !tag.unsignedValue.has_value()){
                    break;
                }
                FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::StructuredScalarEntry entry{};
                entry.name = *name.text;
                entry.hasValueTag = true;
                entry.valueTag = *tag.unsignedValue;
                if(entry.valueTag == 0x4u && value.text.has_value()){
                    entry.encoding = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::TextPayload;
                } else if(entry.valueTag == 0x80u && (value.signedValue.has_value() || value.unsignedValue.has_value())){
                    entry.encoding = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::NumericScalar;
                }
                entry.rawValueType = value.valueType;
                entry.hasTextValue = value.text.has_value();
                if(value.text.has_value()){
                    entry.textValue = *value.text;
                }
                entry.hasSignedValue = value.signedValue.has_value();
                if(value.signedValue.has_value()){
                    entry.signedValue = *value.signedValue;
                }
                entry.hasUnsignedValue = value.unsignedValue.has_value();
                if(value.unsignedValue.has_value()){
                    entry.unsignedValue = *value.unsignedValue;
                }
                entry.valuePreview = value.preview;
                apply_node3_sysinfo_annotation(entry, annotate_as_node3_sysinfo);
                summary.structuredScalars.push_back(std::move(entry));
            }
        };
        std::function<void(const ParsedNamedValueSetItem&)> append_flat_item;
        append_flat_item = [&](const ParsedNamedValueSetItem& item) {
            if(item.text.has_value() || item.signedValue.has_value() || item.unsignedValue.has_value()){
                flat_scalar_items.push_back(item);
            }
            for(const auto& nested : item.groupItems){
                append_flat_item(nested);
            }
        };
        const auto extract_flat_structured_scalars = [&]() {
            if(!summary.structuredScalars.empty() || !summary.entries.empty()){
                return;
            }
            for(std::size_t i = 0; i + 2 < flat_scalar_items.size();){
                const auto& name = flat_scalar_items[i];
                const auto& tag = flat_scalar_items[i + 1];
                const auto& value = flat_scalar_items[i + 2];
                if(name.text.has_value()
                    && tag.unsignedValue.has_value()
                    && (value.text.has_value() || value.signedValue.has_value() || value.unsignedValue.has_value())){
                    FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::StructuredScalarEntry entry{};
                    entry.name = *name.text;
                    entry.hasValueTag = true;
                    entry.valueTag = *tag.unsignedValue;
                    if(entry.valueTag == 0x4u && value.text.has_value()){
                        entry.encoding = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::TextPayload;
                    } else if(entry.valueTag == 0x80u && (value.signedValue.has_value() || value.unsignedValue.has_value())){
                        entry.encoding = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::NumericScalar;
                    }
                    entry.rawValueType = value.valueType;
                    entry.hasTextValue = value.text.has_value();
                    if(value.text.has_value()){
                        entry.textValue = *value.text;
                    }
                    entry.hasSignedValue = value.signedValue.has_value();
                    if(value.signedValue.has_value()){
                        entry.signedValue = *value.signedValue;
                    }
                    entry.hasUnsignedValue = value.unsignedValue.has_value();
                    if(value.unsignedValue.has_value()){
                        entry.unsignedValue = *value.unsignedValue;
                    }
                    entry.valuePreview = value.preview;
                    apply_node3_sysinfo_annotation(entry, annotate_as_node3_sysinfo);
                    summary.structuredScalars.push_back(std::move(entry));
                    i += 3;
                    continue;
                }
                i += 1;
            }
        };

        const bool ok = read_fields_until(
            ar,
            TypeCode::ID::EndPointer,
            [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
                if(is_char_payload(header.payloadType)){
                    std::string text;
                    if(!read_textid_traced(
                           ar,
                           text,
                           trace,
                           name.empty() ? "NamedValueSet.entryName" : "NamedValueSet.string")){
                        return false;
                    }
                    summary.itemCount += 1;
                    append_preview(std::format("text={}", text.empty() ? "<empty>" : text));
                    flat_scalar_items.push_back(ParsedNamedValueSetItem{
                        .preview = std::format("text={}", text.empty() ? "<empty>" : text),
                        .valueType = "text",
                        .text = text,
                    });
                    if(name.empty()){
                        pending_entry_name = std::move(text);
                    }
                    return true;
                }

                if(header.signTypeCode == TypeCode::ID::BeginPointer){
                    FTML::XMSE::ConfigurationSetEntry nested{};
                    if(!read_configuration_pointer_entry(ar, 0, nested, trace, "NamedValueSet.entry")){
                        return false;
                    }
                    append_entry(name, std::move(nested));
                    return true;
                }

                if(header.signTypeCode == TypeCode::ID::BeginGroup){
                    auto item = summarize_current_item();
                    if(!item.has_value()){
                        return false;
                    }
                    append_preview(item->preview);
                    summary.itemCount += 1;
                    append_flat_item(*item);
                    append_structured_scalars(*item);
                    return true;
                }

                const auto preview = summarize_value_item(header);
                append_preview(preview);
                summary.itemCount += 1;
                flat_scalar_items.push_back(ParsedNamedValueSetItem{
                    .preview = preview,
                    .valueType = std::string(item_type_name(header.signTypeCode)),
                    .signedValue = read_signed_value(header),
                    .unsignedValue = read_unsigned_value(header),
                });
                if(!skip_current_item(ar)){
                    return false;
                }
                return true;

            },
            trace);

        if(!ok){
            return false;
        }
        extract_flat_structured_scalars();
        if(!summary.entries.empty() || !summary.preview.empty()){
            out.namedValueSet = std::move(summary);
            return true;
        }
        ar.archive().state() = before_named_parse;
        pending_entry_name.reset();
        ordinal = 0;
        summary = {};
        flat_scalar_items.clear();

        while(true){
            const auto header = ar.next();
            if(!header.hasItem){
                return false;
            }
            if(header.signTypeCode == TypeCode::ID::EndPointer){
                if(!ar.get(TypeCode::ID::EndPointer, 1)){
                    return false;
                }
                extract_flat_structured_scalars();
                if(!summary.entries.empty() || !summary.preview.empty()){
                    out.namedValueSet = std::move(summary);
                }
                return true;
            }
            if(is_char_payload(header.payloadType)){
                std::string text;
                if(!read_textid_traced(ar, text, trace, "NamedValueSet.entryName")){
                    return false;
                }
                summary.itemCount += 1;
                append_preview(std::format("text={}", text.empty() ? "<empty>" : text));
                flat_scalar_items.push_back(ParsedNamedValueSetItem{
                    .preview = std::format("text={}", text.empty() ? "<empty>" : text),
                    .valueType = "text",
                    .text = text,
                });
                pending_entry_name = std::move(text);
                continue;
            }
            if(header.signTypeCode == TypeCode::ID::BeginPointer){
                FTML::XMSE::ConfigurationSetEntry nested{};
                if(!read_configuration_pointer_entry(ar, 0, nested, trace, "NamedValueSet.entry")){
                    return false;
                }
                append_entry({}, std::move(nested));
                continue;
            }
            if(header.signTypeCode == TypeCode::ID::BeginGroup){
                auto item = summarize_current_item();
                if(!item.has_value()){
                    return false;
                }
                append_preview(item->preview);
                summary.itemCount += 1;
                append_flat_item(*item);
                append_structured_scalars(*item);
                continue;
            }
            const auto preview = summarize_value_item(header);
            append_preview(preview);
            summary.itemCount += 1;
            flat_scalar_items.push_back(ParsedNamedValueSetItem{
                .preview = preview,
                .valueType = std::string(item_type_name(header.signTypeCode)),
                .signedValue = read_signed_value(header),
                .unsignedValue = read_unsigned_value(header),
            });
            record_field_trace(trace, "Skip", current_field_name(header), 0);
            if(!skip_current_item(ar)){
                return false;
            }
        }
    };

    auto try_read_named_value_list_summary = [&]() -> bool {
        FTML::XMSE::ConfigurationSetEntry::NamedValueSetEntrySummary::NamedValueListSummary summary{};
        constexpr std::size_t max_preview = 4;
        const auto& dictionaries = ar.archive().state().dictionaries;
        const auto append_preview = [&](std::string text) {
            if(summary.preview.size() < max_preview){
                summary.preview.push_back(std::move(text));
            }
        };
        const auto join_preview = [](const std::vector<std::string>& items) {
            std::string joined;
            for(std::size_t i = 0; i < items.size(); ++i){
                if(!joined.empty()){
                    joined += ", ";
                }
                joined += items[i];
            }
            return joined;
        };
        struct ParsedNamedValueListItem {
            std::string preview;
            std::optional<std::string> text;
            std::optional<std::int32_t> signedValue;
            std::optional<std::uint32_t> unsignedValue;
            OLSA::Container::ResolvedTypeInfo pointerType;
            OLSA::Container::ResolvedTypeInfo expected;
            FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
            std::uint32_t body_offset{};
            std::uint64_t object_id{};
            std::uint64_t tag{};
            std::optional<FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary> configuration;
            std::vector<ParsedNamedValueListItem> groupItems;
        };
        const auto read_signed_value = [&](const ItemHeader& header) -> std::optional<std::int32_t> {
            const auto bytes = as_bytes(header.payloadBytes);
            switch(header.signTypeCode){
            case TypeCode::ID::Int8:
                return static_cast<std::int32_t>(read_little_endian<std::int8_t>(bytes));
            case TypeCode::ID::UInt8:
            case TypeCode::ID::UChar:
                return static_cast<std::int32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::Int16:
                return static_cast<std::int32_t>(read_little_endian<std::int16_t>(bytes));
            case TypeCode::ID::UInt16:
                return static_cast<std::int32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::Int32:
                return read_little_endian<std::int32_t>(bytes);
            case TypeCode::ID::UInt32:
                return static_cast<std::int32_t>(read_little_endian<std::uint32_t>(bytes));
            default:
                return std::nullopt;
            }
        };
        const auto read_unsigned_value = [&](const ItemHeader& header) -> std::optional<std::uint32_t> {
            const auto bytes = as_bytes(header.payloadBytes);
            switch(header.signTypeCode){
            case TypeCode::ID::UInt8:
            case TypeCode::ID::UChar:
                return static_cast<std::uint32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::UInt16:
                return static_cast<std::uint32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::UInt32:
                return read_little_endian<std::uint32_t>(bytes);
            case TypeCode::ID::Int8:
                return static_cast<std::uint32_t>(read_little_endian<std::uint8_t>(bytes));
            case TypeCode::ID::Int16:
                return static_cast<std::uint32_t>(read_little_endian<std::uint16_t>(bytes));
            case TypeCode::ID::Int32:
                return static_cast<std::uint32_t>(read_little_endian<std::int32_t>(bytes));
            default:
                return std::nullopt;
            }
        };
        std::function<std::optional<ParsedNamedValueListItem>(std::size_t)> summarize_current_item;
        summarize_current_item = [&](std::size_t depth) -> std::optional<ParsedNamedValueListItem> {
            const auto header = ar.next();
            if(!header.hasItem){
                return std::nullopt;
            }

            if(is_char_payload(header.payloadType)){
                std::string text;
                if(!ar.gets(text, 0x1e0)){
                    return std::nullopt;
                }
                ParsedNamedValueListItem item{};
                item.text = text;
                item.preview = std::format("text={}", text.empty() ? "<empty>" : text);
                return item;
            }

            if(header.signTypeCode == TypeCode::ID::BeginPointer){
                const auto type = resolve_type_info_from_dictionary(dictionaries, header.tempClassId);
                ParsedNamedValueListItem item{};
                if(is_configuration_like(type.class_name)){
                    FTML::XMSE::ConfigurationSetEntry nested{};
                    if(!read_configuration_pointer_entry(ar, 0, nested, trace, "NamedValueList.entry")){
                        return std::nullopt;
                    }
                    item.pointerType = std::move(nested.type);
                    item.expected = std::move(nested.expected);
                    item.compatibility = nested.compatibility;
                    item.body_offset = nested.body_offset;
                    item.object_id = nested.object_id;
                    item.tag = nested.tag;
                    if(nested.configuration.has_value()){
                        item.configuration = std::move(nested.configuration);
                    }
                } else {
                    item.pointerType = type;
                    if(!skip_current_item(ar)){
                        return std::nullopt;
                    }
                }
                item.preview = std::format(
                    "pointer={}",
                    item.pointerType.display_name().empty() ? std::format("class_id={}", header.tempClassId) : item.pointerType.display_name());
                return item;
            }

            if(header.signTypeCode == TypeCode::ID::BeginObject){
                const auto type = resolve_type_info_from_dictionary(dictionaries, header.tempClassId);
                ParsedNamedValueListItem item{};
                item.pointerType = type;
                item.preview = std::format(
                    "object={}",
                    type.display_name().empty() ? std::format("class_id={}", header.tempClassId) : type.display_name());
                if(!skip_current_item(ar)){
                    return std::nullopt;
                }
                return item;
            }

            if(header.signTypeCode == TypeCode::ID::BeginGroup){
                if(!ar.get(TypeCode::ID::BeginGroup, 1)){
                    return std::nullopt;
                }
                ParsedNamedValueListItem item{};
                std::size_t nested_count = 0;
                std::vector<std::string> nested_preview;
                while(true){
                    const auto before_nested = ar.archive().state();
                    const auto nested_header = ar.next();
                    if(!nested_header.hasItem){
                        return std::nullopt;
                    }
                    if(nested_header.signTypeCode == TypeCode::ID::EndGroup){
                        if(!ar.get(TypeCode::ID::EndGroup, 1)){
                            return std::nullopt;
                        }
                        break;
                    }
                    ar.archive().state() = before_nested;
                    nested_count += 1;
                    auto nested = summarize_current_item(depth + 1);
                    if(!nested.has_value()){
                        return std::nullopt;
                    }
                    if(nested_preview.size() < max_preview){
                        nested_preview.push_back(nested->preview);
                    }
                    item.groupItems.push_back(std::move(*nested));
                }
                item.preview = std::format(
                    "group[{}]=[{}]",
                    nested_count,
                    nested_preview.empty() ? "(none)" : join_preview(nested_preview));
                return item;
            }

            ParsedNamedValueListItem item{};
            item.preview = summarize_value_item(header);
            item.signedValue = read_signed_value(header);
            item.unsignedValue = read_unsigned_value(header);
            if(!skip_current_item(ar)){
                return std::nullopt;
            }
            return item;
        };

        std::size_t top_level_index = 0;
        while(true){
            const auto before_item = ar.archive().state();
            const auto header = ar.next();
            if(!header.hasItem){
                return false;
            }
            if(header.signTypeCode == TypeCode::ID::EndPointer){
                out.namedValueList = std::move(summary);
                return ar.get(TypeCode::ID::EndPointer, 1);
            }
            ar.archive().state() = before_item;

            summary.itemCount += 1;

            auto item = summarize_current_item(0);
            if(!item.has_value()){
                return false;
            }
            append_preview(item->preview);
            if(top_level_index == 0 && item->signedValue.has_value()){
                summary.hasEntriesCount = true;
                summary.entriesCountRaw = *item->signedValue;
            }
            if(!item->groupItems.empty()
                && item->groupItems.size() >= 3
                && item->groupItems[0].text.has_value()
                && item->groupItems[1].unsignedValue.has_value()
                && item->groupItems[2].pointerType.class_id != 0){
                FTML::XMSE::ConfigurationSetEntry::NamedValueSetEntrySummary::NamedValueListSummary::StructuredEntry entry{};
                entry.name = *item->groupItems[0].text;
                entry.hasValueTag = true;
                entry.valueTag = *item->groupItems[1].unsignedValue;
                entry.pointerType = item->groupItems[2].pointerType;
                entry.expected = item->groupItems[2].expected;
                entry.compatibility = item->groupItems[2].compatibility;
                entry.body_offset = item->groupItems[2].body_offset;
                entry.object_id = item->groupItems[2].object_id;
                entry.tag = item->groupItems[2].tag;
                if(item->groupItems[2].configuration.has_value()){
                    entry.configuration = item->groupItems[2].configuration;
                }
                summary.structuredEntries.push_back(std::move(entry));
            }
            top_level_index += 1;
        }
    };

    if(is_configuration_like(out.type.class_name)){
        const auto before_body = ar.archive().state();
        const auto body_header = ar.next();
        if(!body_header.hasItem){
            return false;
        }
        if(body_header.signTypeCode == TypeCode::ID::EndPointer){
            return ar.get(TypeCode::ID::EndPointer, 1);
        }
        ar.archive().state() = before_body;
        if(!body_uses_named_fields(ar, TypeCode::ID::EndPointer)){
            return try_read_dc_configuration_summary_anonymous();
        }
        return try_read_dc_configuration_summary_named();
    }
    if(is_named_value_set_like(out.type.class_name)){
        return try_read_named_value_set_summary();
    }
    if(is_named_value_list_like(out.type.class_name)){
        return try_read_named_value_list_summary();
    }
    return skip_until(ar, TypeCode::ID::EndPointer);
}

[[nodiscard]] bool read_configuration_set(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    FTML::XMSE::ConfigurationSet& out,
    bool& materialized,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    if(!body_uses_named_fields(ar, end_marker)){
        std::size_t matched_fields = 0;
        std::optional<std::string> pending_entry_name;

        if(!read_textid_traced(ar, out.systemID, trace, "SystemID")){
            return false;
        }
        matched_fields += 1;

        while(true){
            const auto header = ar.next();
            if(!header.hasItem){
                return false;
            }
            if(header.signTypeCode == end_marker){
                materialized = matched_fields != 0;
                return ar.get(end_marker, 1) && !pending_entry_name.has_value();
            }
            if(is_char_payload(header.payloadType)){
                std::string entry_name;
                if(!read_textid_traced(ar, entry_name, trace, "ConfigurationSet.entryName")){
                    return false;
                }
                pending_entry_name = std::move(entry_name);
                out.usesPairLayout = true;
                matched_fields += 1;
                continue;
            }
            if(header.signTypeCode == TypeCode::ID::BeginPointer){
                FTML::XMSE::ConfigurationSetEntry entry{};
                entry.key = pending_entry_name.value_or("<legacy>");
                if(!read_configuration_pointer_entry(
                        ar,
                        pending_entry_name.has_value() ? 0x491A1E3Cu : 0x499602DDu,
                        entry,
                        trace,
                        pending_entry_name.has_value() ? "ConfigurationSet.entry" : "ConfigurationSet.legacyRoot")){
                    return false;
                }
                if(pending_entry_name.has_value()){
                    out.entries.push_back(std::move(entry));
                    pending_entry_name.reset();
                    out.usesPairLayout = true;
                } else {
                    out.hasLegacyRoot = true;
                    out.legacyRoot = std::move(entry);
                }
                matched_fields += 1;
                continue;
            }
            record_field_trace(trace, "Skip", current_field_name(header), 0);
            if(!skip_current_item(ar)){
                return false;
            }
        }
    }

    std::size_t matched_fields = 0;
    std::optional<std::string> pending_entry_name;

    const bool ok = read_fields_until(
        ar,
        end_marker,
        [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
            if(name == "SystemID"){
                matched_fields += 1;
                return read_textid_traced(ar, out.systemID, trace, "SystemID");
            }

            if(!name.empty() && header.signTypeCode == TypeCode::ID::BeginPointer){
                FTML::XMSE::ConfigurationSetEntry entry{};
                entry.key = std::string(name);
                if(!read_configuration_pointer_entry(ar, 0x491A1E3Cu, entry, trace, name)){
                    return false;
                }
                out.usesPairLayout = true;
                out.entries.push_back(std::move(entry));
                matched_fields += 1;
                return true;
            }

            if(!name.empty()){
                return std::nullopt;
            }

            if(header.payloadType == TypeCode::ID::Char || header.payloadType == TypeCode::ID::UChar){
                std::string entry_name;
                if(!read_textid_traced(ar, entry_name, trace, "ConfigurationSet.entryName")){
                    return false;
                }
                pending_entry_name = std::move(entry_name);
                out.usesPairLayout = true;
                matched_fields += 1;
                return true;
            }

            if(header.signTypeCode == TypeCode::ID::BeginPointer){
                FTML::XMSE::ConfigurationSetEntry entry{};
                entry.key = pending_entry_name.value_or("<legacy>");
                if(!read_configuration_pointer_entry(
                        ar,
                        pending_entry_name.has_value() ? 0x491A1E3Cu : 0x499602DDu,
                        entry,
                        trace,
                        pending_entry_name.has_value() ? "ConfigurationSet.entry" : "ConfigurationSet.legacyRoot")){
                    return false;
                }

                if(pending_entry_name.has_value()){
                    out.entries.push_back(std::move(entry));
                    pending_entry_name.reset();
                    out.usesPairLayout = true;
                } else {
                    out.hasLegacyRoot = true;
                    out.legacyRoot = std::move(entry);
                }
                matched_fields += 1;
                return true;
            }

            return std::nullopt;
        },
        trace);

    materialized = matched_fields != 0;
    return ok && !pending_entry_name.has_value();
}

[[nodiscard]] bool read_dc_recipe(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    FTML::XMSE::DCRecipe& out,
    bool& materialized,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    if(!body_uses_named_fields(ar, end_marker)){
        return scan_anonymous_payload(
            ar,
            end_marker,
            out,
            materialized,
            trace,
            try_read_dc_recipe_anonymous);
    }
    std::size_t matched_fields = 0;
    const bool ok = read_fields_until(
        ar,
        end_marker,
        [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
            if(name == "NumCyclesRef"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.numCyclesRef);
            }
            if(name == "NumCyclesDark"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.numCyclesDark);
            }
            if(name == "NumCyclesSample"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.numCyclesSample);
            }
            if(name == "SumCycles"){
                matched_fields += 1;
                return read_bool(ar, out.sumCycles);
            }
            if(name == "WRange"){
                matched_fields += 1;
                return read_bool(ar, out.wRangeHasMin)
                    && read_bool(ar, out.wRangeHasMax)
                    && (!out.wRangeHasMin || read_scalar(ar, TypeCode::ID::Float, out.wRangeMin))
                    && (!out.wRangeHasMax || read_scalar(ar, TypeCode::ID::Float, out.wRangeMax));
            }
            if(name == "AnalyzerRef"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::Float, out.analyzerRef);
            }
            if(name == "AnalyzerSample"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::Float, out.analyzerSample);
            }
            if(name == "Rotation"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::Float, out.rotation);
            }
            if(name == "SymThresh"){
                matched_fields += 1;
                return read_bool(ar, out.symThreshHas)
                    && (!out.symThreshHas || read_scalar(ar, TypeCode::ID::Float, out.symThreshValue));
            }
            if(name == "SumsPerCycle"){
                matched_fields += 1;
                return read_bool(ar, out.sumsPerCycleHas)
                    && (!out.sumsPerCycleHas || read_scalar(ar, TypeCode::ID::UInt32, out.sumsPerCycle));
            }
            if(name == "TimingMode"){
                matched_fields += 1;
                return read_bool(ar, out.timingModeHas)
                    && (!out.timingModeHas || read_scalar(ar, TypeCode::ID::UInt32, out.timingMode));
            }
            if(name == "Saturation"){
                matched_fields += 1;
                return read_bool(ar, out.saturationHas)
                    && (!out.saturationHas || read_scalar(ar, TypeCode::ID::UInt32, out.saturation));
            }
            return std::nullopt;
        },
        trace);
    materialized = matched_fields != 0;
    return ok;
}

[[nodiscard]] bool read_dp_recipe(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    FTML::XMSE::DPRecipe& out,
    bool& materialized,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    if(!body_uses_named_fields(ar, end_marker)){
        return scan_anonymous_payload(
            ar,
            end_marker,
            out,
            materialized,
            trace,
            try_read_dp_recipe_anonymous);
    }
    std::size_t matched_fields = 0;
    const bool ok = read_fields_until(
        ar,
        end_marker,
        [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
            if(name == "ConfigApp"){
                matched_fields += 1;
                return read_name_string_traced(ar, out.configApp, trace, "ConfigApp");
            }
            if(name == "Binning"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::Int32, out.binning);
            }
            if(name == "ApplyMultiScanErr"){
                matched_fields += 1;
                return read_bool(ar, out.applyMultiScanErr);
            }
            if(name == "ApplyPSF"){
                matched_fields += 1;
                return read_bool(ar, out.applyPSF);
            }
            if(name == "ApplyLinearity"){
                matched_fields += 1;
                return read_bool(ar, out.applyLinearity);
            }
            if(name == "ApplyDCOffset"){
                matched_fields += 1;
                return read_bool(ar, out.applyDCOffset);
            }
            if(name == "ApplyA0P0Offset"){
                matched_fields += 1;
                return read_bool(ar, out.applyA0P0Offset);
            }
            if(name == "ApplyWShift"){
                matched_fields += 1;
                return read_bool(ar, out.applyWShift);
            }
            if(name == "ApplyTilt"){
                matched_fields += 1;
                return read_bool(ar, out.applyTilt);
            }
            if(name == "ApplyIDN"){
                matched_fields += 1;
                return read_bool(ar, out.applyIDN);
            }
            if(name == "ModelTilt"){
                matched_fields += 1;
                return read_bool(ar, out.modelTilt);
            }
            return std::nullopt;
        },
        trace);
    materialized = matched_fields != 0;
    return ok;
}

[[nodiscard]] bool read_raw_data(
    ::BinaryArchive& ar,
    TypeCode::ID end_marker,
    FTML::XMSE::RawData& out,
    bool& materialized,
    FTML::XMSE::DynamicObjectReadResult* trace) {
    if(!body_uses_named_fields(ar, end_marker)){
        return scan_anonymous_payload(
            ar,
            end_marker,
            out,
            materialized,
            trace,
            try_read_raw_data_anonymous);
    }
    std::size_t matched_fields = 0;
    const bool ok = read_fields_until(
        ar,
        end_marker,
        [&](std::string_view name, const ItemHeader& header) -> std::optional<bool> {
            if(name == "NumSums"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.numSums);
            }
            if(name == "SumsPerCycle"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.sumsPerCycle);
            }
            if(name == "TimingMode"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.timingMode);
            }
            if(name == "NumPixel"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::Int32, out.numPixel);
            }
            if(name == "TurnsPerCycle"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.turnsPerCycle0)
                    && read_scalar(ar, TypeCode::ID::UInt32, out.turnsPerCycle1);
            }
            if(name == "NumBM"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.numBM);
            }
            if(name == "FirstSum"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.firstSum);
            }
            if(name == "FirstAcqSum"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.firstAcqSum);
            }
            if(name == "PixelRange"){
                matched_fields += 1;
                return read_bool(ar, out.pixelRangeHasMin)
                    && read_bool(ar, out.pixelRangeHasMax)
                    && (!out.pixelRangeHasMin || read_scalar(ar, TypeCode::ID::Int32, out.pixelRangeMin))
                    && (!out.pixelRangeHasMax || read_scalar(ar, TypeCode::ID::Int32, out.pixelRangeMax));
            }
            if(name == "ClkPeriod"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.clkPeriod);
            }
            if(name == "Enc1Lines"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.enc1Lines);
            }
            if(name == "Enc2Lines"){
                matched_fields += 1;
                return read_scalar(ar, TypeCode::ID::UInt32, out.enc2Lines);
            }
            if(name == "Sig"){
                if(header.signTypeCode != TypeCode::ID::BeginPointer){
                    return false;
                }
                FTML::XMSE::RawData::UInt32ArrayPayload value{};
                if(!read_known_uint32_array_pointer_body(ar, header.tempClassId, header.tempObjectId, value)){
                    return std::nullopt;
                }
                record_field_trace(trace, "Array", "Sig", 0);
                matched_fields += 1;
                out.sig = std::move(value);
                return true;
            }
            if(name == "Enc1"){
                if(header.signTypeCode != TypeCode::ID::BeginPointer){
                    return false;
                }
                FTML::XMSE::RawData::UInt32ArrayPayload value{};
                if(!read_known_uint32_array_pointer_body(ar, header.tempClassId, header.tempObjectId, value)){
                    return std::nullopt;
                }
                record_field_trace(trace, "Array", "Enc1", 0);
                matched_fields += 1;
                out.enc1 = std::move(value);
                return true;
            }
            if(name == "Enc2"){
                if(header.signTypeCode != TypeCode::ID::BeginPointer){
                    return false;
                }
                FTML::XMSE::RawData::UInt32ArrayPayload value{};
                if(!read_known_uint32_array_pointer_body(ar, header.tempClassId, header.tempObjectId, value)){
                    return std::nullopt;
                }
                record_field_trace(trace, "Array", "Enc2", 0);
                matched_fields += 1;
                out.enc2 = std::move(value);
                return true;
            }
            if(name == "Clk"){
                if(header.signTypeCode != TypeCode::ID::BeginPointer){
                    return false;
                }
                FTML::XMSE::RawData::UInt32ArrayPayload value{};
                if(!read_known_uint32_array_pointer_body(ar, header.tempClassId, header.tempObjectId, value)){
                    return std::nullopt;
                }
                record_field_trace(trace, "Array", "Clk", 0);
                matched_fields += 1;
                out.clk = std::move(value);
                return true;
            }
            if(name == "BM"){
                if(out.numBM == 0){
                    return std::nullopt;
                }
                if(header.signTypeCode != TypeCode::ID::BeginPointer){
                    return false;
                }
                FTML::XMSE::RawData::UInt32ArrayPayload value{};
                if(!read_known_uint32_array_pointer_body(ar, header.tempClassId, header.tempObjectId, value)){
                    return std::nullopt;
                }
                record_field_trace(trace, "Array", "BM", 0);
                matched_fields += 1;
                out.bm = std::move(value);
                return true;
            }
            return std::nullopt;
        },
        trace);
    materialized = matched_fields != 0;
    return ok;
}
}  // namespace FTML::XMSE::detail

bool FTML::XMSE::read_dynamic(::BinaryArchive& ar, DynamicObjectReadResult& out) {
    out = {};
    const auto header = ar.next();
    if(!header.hasItem){
        out.detail = "no item available";
        return false;
    }
    out.object_id = header.tempObjectId;
    out.tag = header.hasTag ? header.tag : 0;

    TypeCode::ID end_marker{};
    if(header.signTypeCode == TypeCode::ID::BeginPointer){
        out.started_from_pointer = true;
        FTML::SmartPointer::ExtractResult extracted{};
        if(!FTML::SmartPointer::extract(ar, 0, extracted) || !extracted.recognized()){
            out.status = DynamicReadStatus::UnrecognizedType;
            out.detail = "pointer type could not be resolved";
            return false;
        }
        out.type = extracted.actual;
        out.body_offset = ar.archive().state().input.offset;
        end_marker = TypeCode::ID::EndPointer;
    } else if(header.signTypeCode == TypeCode::ID::BeginObject){
        if(!ar.getObject()){
            return false;
        }
        const auto& resolved = ar.archive().state().parsing.resolvedType;
        if(!resolved.has_value()){
            out.status = DynamicReadStatus::UnrecognizedType;
            out.detail = "object type could not be resolved";
            return false;
        }
        out.type = *resolved;
        out.body_offset = ar.archive().state().input.offset;
        end_marker = TypeCode::ID::EndObject;
    } else {
        out.status = DynamicReadStatus::InvalidStart;
        out.detail = "current item is neither BeginPointer nor BeginObject";
        return false;
    }

    if(out.type.class_name == "FTML::XMSE::DCRecipe"){
        DCRecipe value{};
        bool materialized = false;
        if(!detail::read_dc_recipe(ar, end_marker, value, materialized, &out)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "failed while reading FTML::XMSE::DCRecipe body";
            return false;
        }
        if(materialized){
            out.kind = DynamicObjectKind::DCRecipe;
            out.value = std::move(value);
            out.status = DynamicReadStatus::Handled;
            out.detail = "materialized FTML::XMSE::DCRecipe from recognized fields";
        } else {
            out.status = DynamicReadStatus::NoMaterializedFields;
            out.detail = "recognized FTML::XMSE::DCRecipe but no known fields matched";
        }
        return true;
    }
    if(out.type.class_name == "FTML::XMSE::DPRecipe"){
        DPRecipe value{};
        bool materialized = false;
        if(!detail::read_dp_recipe(ar, end_marker, value, materialized, &out)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "failed while reading FTML::XMSE::DPRecipe body";
            return false;
        }
        if(materialized){
            out.kind = DynamicObjectKind::DPRecipe;
            out.value = std::move(value);
            out.status = DynamicReadStatus::Handled;
            out.detail = "materialized FTML::XMSE::DPRecipe from recognized fields";
        } else {
            out.status = DynamicReadStatus::NoMaterializedFields;
            out.detail = "recognized FTML::XMSE::DPRecipe but no known fields matched";
        }
        return true;
    }
    if(out.type.class_name == "FTML::XMSE::RawData"){
        RawData value{};
        bool materialized = false;
        if(!detail::read_raw_data(ar, end_marker, value, materialized, &out)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "failed while reading FTML::XMSE::RawData body";
            return false;
        }
        if(materialized){
            out.kind = DynamicObjectKind::RawData;
            out.detail = std::format(
                "materialized FTML::XMSE::RawData from recognized fields; {}",
                detail::summarize_raw_data_shape_semantics(value));
            out.value = std::move(value);
            out.status = DynamicReadStatus::Handled;
        } else {
            out.status = DynamicReadStatus::NoMaterializedFields;
            out.detail = "recognized FTML::XMSE::RawData but no known fields matched";
        }
        return true;
    }
    if(out.type.class_name == "FTML::XMSE::RawDataSet"){
        RawDataSet value{};
        if(!value.get(ar)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "failed while reading FTML::XMSE::RawDataSet body";
            return false;
        }
        if(!detail::read_expected_item(ar, end_marker)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "missing RawDataSet end marker";
            return false;
        }

        out.kind = DynamicObjectKind::RawDataSet;
        out.value = std::move(value);
        out.status = DynamicReadStatus::Handled;
        out.detail = "materialized FTML::XMSE::RawDataSet top-level summary";
        return true;
    }
    if(out.type.class_name == "FTML::XMSE::ConfigurationSet"){
        ConfigurationSet value{};
        bool materialized = false;
        if(!detail::read_configuration_set(ar, end_marker, value, materialized, &out)){
            out.status = DynamicReadStatus::ParseFailed;
            out.detail = "failed while reading FTML::XMSE::ConfigurationSet body";
            return false;
        }
        if(materialized){
            out.kind = DynamicObjectKind::ConfigurationSet;
            out.detail = std::format(
                "materialized FTML::XMSE::ConfigurationSet systemID={} entries={} legacy_root={}",
                value.systemID.empty() ? "<empty>" : value.systemID,
                value.entries.size(),
                value.hasLegacyRoot ? "true" : "false");
            out.value = std::move(value);
            out.status = DynamicReadStatus::Handled;
        } else {
            out.status = DynamicReadStatus::NoMaterializedFields;
            out.detail = "recognized FTML::XMSE::ConfigurationSet but no known fields matched";
        }
        return true;
    }

    out.status = DynamicReadStatus::UnsupportedType;
    out.detail = "recognized type is outside the current XMSE targeted reader set";
    return detail::skip_until(ar, end_marker);
}

bool FTML::XMSE::is_supported_dynamic_type(std::string_view class_name) {
    return class_name == "FTML::XMSE::DCRecipe"
        || class_name == "FTML::XMSE::DPRecipe"
        || class_name == "FTML::XMSE::RawData"
        || class_name == "FTML::XMSE::RawDataSet"
        || class_name == "FTML::XMSE::ConfigurationSet";
}

bool FTML::XMSE::is_xmse_dynamic_type(std::string_view class_name) {
    return class_name.starts_with("FTML::XMSE::");
}

std::vector<FTML::XMSE::DynamicObjectReadResult> FTML::XMSE::collect_recognized_dynamic_objects(::BinaryArchive& ar) {
    std::vector<DynamicObjectReadResult> results;

    while(true){
        const auto header = ar.next();
        if(!header.hasItem){
            break;
        }

        if(header.signTypeCode == TypeCode::ID::BeginPointer
            || header.signTypeCode == TypeCode::ID::BeginObject){
            DynamicObjectReadResult result{};
            if(read_dynamic(ar, result) && result.recognized() && is_xmse_dynamic_type(result.type.class_name)){
                results.push_back(std::move(result));
            }
            continue;
        }

        ar.doGet(TypeCode::ID::None);
    }

    return results;
}

std::vector<FTML::XMSE::DynamicObjectReadResult> FTML::XMSE::collect_supported_dynamic_objects(::BinaryArchive& ar) {
    auto results = collect_recognized_dynamic_objects(ar);
    std::erase_if(results, [](const auto& result) {
        return !is_supported_dynamic_type(result.type.class_name);
    });
    return results;
}

bool FTML::XMSE::RawDataSet::get(::BinaryArchive& ar) {
    const auto read_name_like_current_item = [&](std::string& out) {
        const auto current = ar.next();
        if(!current.hasItem){
            return false;
        }
        if(detail::is_char_payload(current.payloadType)){
            return ar.gets(out, 0x1e0);
        }
        if(current.signTypeCode == TypeCode::ID::BeginGroup){
            return ar.get(TypeCode::ID::BeginGroup, 1)
                && detail::read_name_string_body_until(ar, TypeCode::ID::EndGroup, out);
        }
        if(current.signTypeCode == TypeCode::ID::BeginObject){
            return ar.getObject()
                && detail::read_name_string_body_until(ar, TypeCode::ID::EndObject, out);
        }
        return false;
    };
    const auto read_pointer_slot = [&](std::string_view name, bool& present) {
        const auto header = ar.next();
        if(!header.hasItem || detail::current_field_name(header) != name){
            return false;
        }
        present = header.signTypeCode == TypeCode::ID::BeginPointer;
        return detail::skip_current_item(ar);
    };
    const auto read_name_string_slot = [&](std::string_view name, std::string& out) {
        const auto header = ar.next();
        if(!header.hasItem || detail::current_field_name(header) != name){
            return false;
        }
        if(detail::looks_like_name_string_header(header)){
            return detail::read_name_string_traced(ar, out, nullptr, name);
        }
        std::uint8_t ignored{};
        out.clear();
        return detail::read_scalar(ar, TypeCode::ID::UInt8, ignored);
    };
    const auto read_pointer_slot_anonymous = [&](bool& present) {
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode != TypeCode::ID::BeginPointer){
            present = false;
            return false;
        }
        present = true;
        return detail::skip_current_item(ar);
    };
    const auto read_flag_value_anonymous = [&](FTML::FlagValue<float>& out) {
        if(!detail::read_bool(ar, out.has)){
            return false;
        }
        if(out.has){
            return detail::read_scalar(ar, TypeCode::ID::Float, out.value);
        }
        out.value = 0.0f;
        return true;
    };
    const auto read_filter_slot_anonymous = [&](std::string& out) {
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(detail::looks_like_name_string_header(header)){
            return read_name_like_current_item(out);
        }
        if(header.signTypeCode == TypeCode::ID::Int8
            || header.signTypeCode == TypeCode::ID::UInt8
            || header.signTypeCode == TypeCode::ID::Char
            || header.signTypeCode == TypeCode::ID::UChar){
            out.clear();
            return detail::skip_current_item(ar);
        }
        return false;
    };
    const auto read_version_anonymous = [&]() {
        const auto header = ar.next();
        if(!header.hasItem){
            return false;
        }
        if(!ar.get(header.signTypeCode, 1)){
            return false;
        }

        const auto& payload = ar.archive().state().parsing.itemHeader.payloadBytes;
        switch(header.signTypeCode){
        case TypeCode::ID::Int8:
            if(payload.size() != sizeof(std::int8_t)){
                return false;
            }
            version = static_cast<std::int16_t>(read_little_endian<std::int8_t>(as_bytes(payload)));
            return true;
        case TypeCode::ID::UInt8:
        case TypeCode::ID::UChar:
            if(payload.size() != sizeof(std::uint8_t)){
                return false;
            }
            version = static_cast<std::int16_t>(read_little_endian<std::uint8_t>(as_bytes(payload)));
            return true;
        case TypeCode::ID::Int16:
            if(payload.size() != sizeof(std::int16_t)){
                return false;
            }
            version = read_little_endian<std::int16_t>(as_bytes(payload));
            return true;
        case TypeCode::ID::UInt16:
            if(payload.size() != sizeof(std::uint16_t)){
                return false;
            }
            version = static_cast<std::int16_t>(read_little_endian<std::uint16_t>(as_bytes(payload)));
            return true;
        default:
            return false;
        }
    };
    dcRawDataSet.sampleID.clear();
    hasSysInfo = false;
    sysInfo = {};
    tiltSummary.reset();

    const auto maybe_read_dc_rawdataset_prefix = [&](bool assign) {
        auto saved_state = ar.archive().state();
        std::string parsed_sample_id;
        FTML::XMSE::ConfigurationSetEntry parsed_sys_info{};
        const auto sample_id = ar.next();
        if(!sample_id.hasItem){
            return false;
        }
        if(detail::current_field_name(sample_id) == "Version"){
            ar.archive().state() = saved_state;
            return true;
        }

        const auto sample_name = detail::current_field_name(sample_id);
        if(!sample_name.empty() && sample_name != "SampleID"){
            ar.archive().state() = saved_state;
            return true;
        }
        if(!read_name_like_current_item(parsed_sample_id)){
            ar.archive().state() = saved_state;
            return true;
        }

        const auto before_sys_info = ar.archive().state();
        const auto sys_info = ar.next();
        if(!sys_info.hasItem){
            ar.archive().state() = saved_state;
            return true;
        }
        const auto sys_info_name = detail::current_field_name(sys_info);
        if(!sys_info_name.empty() && sys_info_name != "SysInfo"){
            ar.archive().state() = saved_state;
            return true;
        }
        if(sys_info.signTypeCode != TypeCode::ID::BeginPointer){
            ar.archive().state() = saved_state;
            return true;
        }
        ar.archive().state() = before_sys_info;
        bool parsed_sys_info_summary = detail::read_configuration_pointer_entry(
            ar,
            0x499602DDu,
            parsed_sys_info,
            nullptr,
            "RawDataSet.SysInfo");
        if(!parsed_sys_info_summary){
            ar.archive().state() = before_sys_info;
            if(!detail::skip_current_item(ar)){
                ar.archive().state() = saved_state;
                return true;
            }
        }
        if(assign){
            dcRawDataSet.sampleID = std::move(parsed_sample_id);
            hasSysInfo = parsed_sys_info_summary;
            if(parsed_sys_info_summary){
                sysInfo = std::move(parsed_sys_info);
            }
        }
        return true;
    };

    const auto initial_state = ar.archive().state();
    bool use_named_layout = false;
    if(maybe_read_dc_rawdataset_prefix(false)){
        const auto after_dc_prefix = ar.archive().state();
        const auto next = ar.next();
        use_named_layout = next.hasItem && !detail::current_field_name(next).empty();
        ar.archive().state() = after_dc_prefix;
    }
    ar.archive().state() = initial_state;

    if(!use_named_layout){
        const auto maybe_read_dc_rawdataset_prefix_anonymous = [&]() {
            auto saved_state = ar.archive().state();
            std::string parsed_sample_id;
            FTML::XMSE::ConfigurationSetEntry parsed_sys_info{};
            bool parsed_sys_info_summary = false;

            const auto sample_id = ar.next();
            if(!sample_id.hasItem){
                return false;
            }
            if(detail::looks_like_name_string_header(sample_id)){
                if(!read_name_like_current_item(parsed_sample_id)){
                    ar.archive().state() = saved_state;
                    return true;
                }
            } else if(sample_id.signTypeCode == TypeCode::ID::Int8
                || sample_id.signTypeCode == TypeCode::ID::UInt8
                || sample_id.signTypeCode == TypeCode::ID::Char
                || sample_id.signTypeCode == TypeCode::ID::UChar){
                if(!detail::skip_current_item(ar)){
                    return false;
                }
            } else {
                ar.archive().state() = saved_state;
                return true;
            }

            const auto before_sys_info = ar.archive().state();
            const auto sys_info = ar.next();
            if(!sys_info.hasItem){
                return false;
            }
            if(sys_info.signTypeCode != TypeCode::ID::BeginPointer){
                ar.archive().state() = saved_state;
                return true;
            }
            ar.archive().state() = before_sys_info;
            parsed_sys_info_summary = detail::read_configuration_pointer_entry(
                ar,
                0x499602DDu,
                parsed_sys_info,
                nullptr,
                "RawDataSet.SysInfo");
            if(!parsed_sys_info_summary){
                ar.archive().state() = before_sys_info;
                if(!detail::skip_current_item(ar)){
                    return false;
                }
            }

            dcRawDataSet.sampleID = std::move(parsed_sample_id);
            hasSysInfo = parsed_sys_info_summary;
            if(parsed_sys_info_summary){
                sysInfo = std::move(parsed_sys_info);
            }
            return true;
        };

        if(!maybe_read_dc_rawdataset_prefix_anonymous()){
            return false;
        }
        if(!read_version_anonymous()){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasDCRecipe)){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasDPRecipe)){
            return false;
        }
        if(!read_flag_value_anonymous(rotation)){
            return false;
        }
        if(!read_flag_value_anonymous(analyzerRef)){
            return false;
        }
        if(!read_flag_value_anonymous(analyzerSample)){
            return false;
        }
        if(!detail::read_bool(ar, hasTilt)){
            return false;
        }
        if(hasTilt){
            FTML::XMSE::RawDataSet::TiltSummary parsed_tilt{};
            if(!detail::read_tilt_object(ar, parsed_tilt)){
                return false;
            }
            tiltSummary = std::move(parsed_tilt);
            tiltValue = 0.0f;
        } else {
            tiltValue = 0.0f;
        }
        if(!read_flag_value_anonymous(pixShiftRef)){
            return false;
        }
        if(!read_flag_value_anonymous(pixShiftSample)){
            return false;
        }
        if(!read_filter_slot_anonymous(filterRefText)){
            return false;
        }
        if(!read_filter_slot_anonymous(filterSampleText)){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasRef)){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasDark)){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasSample)){
            return false;
        }
        if(!read_pointer_slot_anonymous(hasConfigSet)){
            return false;
        }
        return true;
    }

    if(!maybe_read_dc_rawdataset_prefix(true)){
        return false;
    }

    if(!detail::expect_field(ar, "Version") || !detail::read_scalar(ar, TypeCode::ID::Int16, version)){
        return false;
    }

    if(!read_pointer_slot("DCRecipe", hasDCRecipe)){
        return false;
    }
    if(!read_pointer_slot("DPRecipe", hasDPRecipe)){
        return false;
    }

    if(!detail::read_flag_value(ar, "Rotation", rotation)){
        return false;
    }
    if(!detail::read_flag_value(ar, "AnalyzerRef", analyzerRef)){
        return false;
    }
    if(!detail::read_flag_value(ar, "AnalyzerSample", analyzerSample)){
        return false;
    }

    if(!detail::expect_field(ar, "Tilt")){
        return false;
    }
    if(!detail::read_bool(ar, hasTilt)){
        return false;
    }
    if(hasTilt){
        const auto before_tilt_value = ar.archive().state();
        const auto tilt_item = ar.next();
        if(!tilt_item.hasItem){
            return false;
        }
        ar.archive().state() = before_tilt_value;
        if(tilt_item.signTypeCode == TypeCode::ID::BeginObject){
            FTML::XMSE::RawDataSet::TiltSummary parsed_tilt{};
            if(!detail::read_tilt_object(ar, parsed_tilt)){
                return false;
            }
            tiltSummary = std::move(parsed_tilt);
            tiltValue = 0.0f;
        } else {
            if(!detail::read_scalar(ar, TypeCode::ID::Float, tiltValue)){
                return false;
            }
            tiltSummary.reset();
        }
    } else {
        tiltValue = 0.0f;
        tiltSummary.reset();
    }

    if(!detail::read_flag_value(ar, "PixShiftRef", pixShiftRef)){
        return false;
    }
    if(!detail::read_flag_value(ar, "PixShiftSample", pixShiftSample)){
        return false;
    }

    if(!read_name_string_slot("FilterRef", filterRefText)){
        return false;
    }
    if(!read_name_string_slot("FilterSample", filterSampleText)){
        return false;
    }
    if(!read_pointer_slot("Ref", hasRef)){
        return false;
    }
    if(!read_pointer_slot("Dark", hasDark)){
        return false;
    }
    if(!read_pointer_slot("Sample", hasSample)){
        return false;
    }
    if(!read_pointer_slot("ConfigSet", hasConfigSet)){
        return false;
    }

    return true;
}
