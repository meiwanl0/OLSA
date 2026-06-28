#include "../include/BinaryArchive.h"
#include "../include/BinaryFileReader.h"
#include "../include/SmartPointer.h"
#include "../include/XMSERawDataSet.h"
#include "../include/archive/TypeCompatibility.h"
#include "../include/protocol/ByteUtil.h"
#include "../tests/UnitTest.h"

#include <array>
#include <cstring>
#include <format>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <set>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {
[[nodiscard]] std::string summarize_configuration_entry(const FTML::XMSE::ConfigurationSetEntry& entry);

[[nodiscard]] std::string_view dynamic_read_status_name(FTML::XMSE::DynamicReadStatus status) {
    switch(status){
    case FTML::XMSE::DynamicReadStatus::InvalidStart:
        return "InvalidStart";
    case FTML::XMSE::DynamicReadStatus::UnrecognizedType:
        return "UnrecognizedType";
    case FTML::XMSE::DynamicReadStatus::UnsupportedType:
        return "UnsupportedType";
    case FTML::XMSE::DynamicReadStatus::NoMaterializedFields:
        return "NoMaterializedFields";
    case FTML::XMSE::DynamicReadStatus::Handled:
        return "Handled";
    case FTML::XMSE::DynamicReadStatus::ParseFailed:
        return "ParseFailed";
    }
    return "Unknown";
}

[[nodiscard]] std::string summarize_observed_fields(const FTML::XMSE::DynamicObjectReadResult& result) {
    if(result.observed_fields.empty()){
        return "(none)";
    }

    std::string summary;
    constexpr std::size_t max_fields = 200;
    for(std::size_t i = 0; i < result.observed_fields.size() && i < max_fields; ++i){
        const auto& item = result.observed_fields[i];
        if(!summary.empty()){
            summary += ", ";
        }
        summary += std::format("{}:{}:{}", item.depth, item.item_kind, item.name.empty() ? "<anonymous>" : item.name);
    }
    if(result.observed_fields.size() > max_fields){
        summary += ", ...";
    }
    return summary;
}

[[nodiscard]] std::string_view compatibility_name(FTML::SmartPointer::Compatibility compatibility) {
    switch(compatibility){
    case FTML::SmartPointer::Compatibility::NoExpectation:
        return "NoExpectation";
    case FTML::SmartPointer::Compatibility::Exact:
        return "Exact";
    case FTML::SmartPointer::Compatibility::Derived:
        return "Derived";
    case FTML::SmartPointer::Compatibility::Different:
        return "Different";
    case FTML::SmartPointer::Compatibility::UnknownExpected:
        return "UnknownExpected";
    default:
        return "Unresolved";
    }
}

[[nodiscard]] std::string_view observed_value_encoding_name(
    FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding encoding) {
    switch(encoding){
    case FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::TextPayload:
        return "TextPayload";
    case FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::NumericScalar:
        return "NumericScalar";
    default:
        return "Unknown";
    }
}

[[nodiscard]] std::string_view semantic_status_name(
    FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus status) {
    using SemanticStatus = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus;
    switch(status){
    case SemanticStatus::FullyRestored:
        return "FullyRestored";
    case SemanticStatus::ClosedWithCaveat:
        return "ClosedWithCaveat";
    case SemanticStatus::Opaque:
        return "Opaque";
    default:
        return "Unclassified";
    }
}

[[nodiscard]] std::string summarize_resolved_type(const OLSA::Container::ResolvedTypeInfo& info) {
    if(info.class_id == 0){
        return "none";
    }

    const auto name = info.display_name();
    if(name.empty()){
        return std::format("class_id={}", info.class_id);
    }
    return std::format("{}(class_id={})", name, info.class_id);
}

[[nodiscard]] std::string summarize_name_string(std::string_view raw) {
    if(raw.empty()){
        return "<empty>";
    }

    std::string summary;
    bool saw_separator = false;
    for(char ch : raw){
        if(ch == '\0'){
            saw_separator = true;
            if(!summary.empty() && summary.back() != '|'){
                summary += '|';
            }
            continue;
        }
        summary += ch;
    }

    if(summary.empty()){
        return saw_separator ? "<segments>" : "<empty>";
    }
    return summary;
}

[[nodiscard]] std::string summarize_tilt_units(const FTML::XMSE::RawDataSet::TiltSummary& tilt) {
    if(tilt.hasUnitsText && tilt.hasUnitsRaw){
        return std::format("{}(raw=0x{:X})", summarize_name_string(tilt.unitsText), tilt.unitsRaw);
    }
    if(tilt.hasUnitsText){
        return summarize_name_string(tilt.unitsText);
    }
    if(tilt.hasUnitsRaw){
        return std::format("0x{:X}", tilt.unitsRaw);
    }
    return "<none>";
}

[[nodiscard]] std::string summarize_tilt_summary(const FTML::XMSE::RawDataSet& value) {
    if(!value.hasTilt){
        return "<absent>";
    }
    if(!value.tiltSummary.has_value()){
        return std::format("legacy-float({:.6f})", value.tiltValue);
    }
    const auto& tilt = *value.tiltSummary;
    return std::format(
        "object(theta={} phi={} units={})",
        tilt.hasTheta ? std::format("{:.6f}", tilt.theta) : std::string{"<none>"},
        tilt.hasPhi ? std::format("{:.6f}", tilt.phi) : std::string{"<none>"},
        summarize_tilt_units(tilt));
}

[[nodiscard]] std::string summarize_structured_scalar_raw_value(
    const FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::StructuredScalarEntry& scalar) {
    if(scalar.hasTextValue){
        return scalar.textValue.empty() ? std::string{"<empty>"} : scalar.textValue;
    }
    if(scalar.hasSignedValue){
        return std::to_string(scalar.signedValue);
    }
    if(scalar.hasUnsignedValue){
        return std::to_string(scalar.unsignedValue);
    }
    return scalar.valuePreview.empty() ? std::string{"<none>"} : scalar.valuePreview;
}

[[nodiscard]] std::string summarize_structured_scalar_line(
    const FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::StructuredScalarEntry& scalar) {
    std::string summary = std::format(
        "{}={} [meaning={} type={} encoding={}]",
        scalar.name.empty() ? "<unnamed>" : scalar.name,
        summarize_structured_scalar_raw_value(scalar),
        scalar.semanticMeaning.empty() ? "<unspecified>" : scalar.semanticMeaning,
        scalar.rawValueType.empty() ? "<none>" : scalar.rawValueType,
        observed_value_encoding_name(scalar.encoding));
    if(!scalar.semanticNote.empty()){
        summary += std::format(" note={}", scalar.semanticNote);
    }
    return summary;
}

void dump_node3_sysinfo_block(const FTML::XMSE::ConfigurationSetEntry& entry, std::string_view indent) {
    using NamedValueSetSummary = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary;
    if(!entry.namedValueSet.has_value()){
        std::cout << std::format("{}sysInfo={}\n", indent, summarize_configuration_entry(entry));
        return;
    }

    const auto& summary = *entry.namedValueSet;
    std::cout << std::format(
        "{}sysInfo: items={} entries={} structuredScalars={}\n",
        indent,
        summary.itemCount,
        summary.entries.size(),
        summary.structuredScalars.size());

    if(!summary.preview.empty()){
        std::string preview_text;
        constexpr std::size_t max_preview = 6;
        for(std::size_t i = 0; i < summary.preview.size() && i < max_preview; ++i){
            if(!preview_text.empty()){
                preview_text += ", ";
            }
            preview_text += summary.preview[i];
        }
        if(summary.preview.size() > max_preview){
            preview_text += ", ...";
        }
        std::cout << std::format("{}  preview=[{}]\n", indent, preview_text);
    }

    constexpr auto groups = std::to_array<std::pair<NamedValueSetSummary::SemanticStatus, std::string_view>>({
        {NamedValueSetSummary::SemanticStatus::FullyRestored, "fullyRestored"},
        {NamedValueSetSummary::SemanticStatus::ClosedWithCaveat, "closedWithCaveat"},
        {NamedValueSetSummary::SemanticStatus::Opaque, "opaque"},
        {NamedValueSetSummary::SemanticStatus::Unclassified, "unclassified"},
    });

    for(const auto& [status, label] : groups){
        std::size_t count = 0;
        for(const auto& scalar : summary.structuredScalars){
            if(scalar.semanticStatus != status){
                continue;
            }
            if(count == 0){
                std::cout << std::format("{}  {}:\n", indent, label);
            }
            std::cout << std::format("{}    - {}\n", indent, summarize_structured_scalar_line(scalar));
            count += 1;
        }
    }
}

[[nodiscard]] std::string format_std_config_value(std::uint32_t raw, std::string_view name) {
    if(name.empty()){
        return std::format("{}", raw);
    }
    return std::format("{}({})", raw, name);
}

[[nodiscard]] std::string summarize_uint32_array_payload(
    const std::optional<FTML::XMSE::RawData::UInt32ArrayPayload>& payload) {
    if(!payload.has_value()){
        return "none";
    }

    const auto preview_count = std::min<std::size_t>(payload->values.size(), 3);
    std::string preview;
    for(std::size_t i = 0; i < preview_count; ++i){
        if(!preview.empty()){
            preview += ",";
        }
        preview += std::to_string(payload->values[i]);
    }
    if(payload->values.size() > preview_count){
        preview += ",...";
    }
    if(preview.empty()){
        preview = "shell";
    }

    if(payload->is_2d){
        return std::format("{}x{} [{}]", payload->dim0, payload->dim1, preview);
    }
    return std::format("{} [{}]", payload->element_count(), preview);
}

[[nodiscard]] std::string summarize_configuration_summary(
    const FTML::XMSE::ConfigurationSetEntry::ConfigurationSummary& summary) {
    std::string coverage;
    const auto append_coverage = [&](std::string_view label) {
        if(!coverage.empty()){
            coverage += ",";
        }
        coverage += label;
    };
    if(!summary.system.empty()){
        append_coverage("system");
    }
    if(!summary.comment.empty()){
        append_coverage("comment");
    }
    if(summary.hasTimestamp){
        append_coverage("timestamp");
    }
    if(!summary.baseName.empty()){
        append_coverage("base");
    }
    if(summary.hasConfigInfo){
        append_coverage("configInfo");
    }
    if(summary.dcTrailerSummary.has_value()){
        append_coverage("dcTrailer");
    }
    if(summary.hasXMSETail){
        append_coverage("xmseTail");
    }
    const auto timestamp_text = summary.hasTimestamp
        ? std::format(
              "{:04d}-{:02d}-{:02d} {:02d}:{:02d} raw={}",
              summary.timestampYear,
              summary.timestampMonth,
              summary.timestampDay,
              summary.timestampHour,
              summary.timestampMinute,
              summary.timestampSubMinuteRaw)
        : std::string{"<none>"};
    std::string config_info_entries_summary;
    if(summary.configInfoSummary.has_value() && !summary.configInfoSummary->entries.empty()){
        constexpr std::size_t max_entries = 2;
        std::string entries_preview;
        for(std::size_t i = 0; i < summary.configInfoSummary->entries.size() && i < max_entries; ++i){
            const auto& entry = summary.configInfoSummary->entries[i];
            if(!entries_preview.empty()){
                entries_preview += ", ";
            }
            entries_preview += std::format(
                "{}#{}:{}<= {}:{} oid={} body=0x{:X}",
                entry.key.empty() ? "<unnamed>" : entry.key,
                entry.ordinal,
                entry.type.display_name().empty() ? std::format("class_id={}", entry.type.class_id) : entry.type.display_name(),
                entry.expected.display_name().empty()
                    ? std::format("class_id={}", entry.expected.class_id)
                    : entry.expected.display_name(),
                compatibility_name(entry.compatibility),
                entry.object_id,
                entry.body_offset);
        }
        if(summary.configInfoSummary->entries.size() > max_entries){
            entries_preview += ", ...";
        }
        config_info_entries_summary = std::format(
            " configInfoEntries[{}]=[{}]",
            summary.configInfoSummary->entries.size(),
            entries_preview.empty() ? "(none)" : entries_preview);
    }
    std::string dc_trailer_summary;
    if(summary.dcTrailerSummary.has_value()){
        const auto& dc = *summary.dcTrailerSummary;
        dc_trailer_summary = std::format(
            " dc(extracted={} calibrations={})",
            dc.hasExtracted ? format_std_config_value(dc.extracted, dc.extractedStdConfigName) : std::string{"<none>"},
            dc.hasCalibrationsGroup ? std::format("{}", dc.calibrationEntries.size()) : std::string{"<none>"});
        if(!dc.calibrationEntries.empty()){
            const std::size_t max_calibration_entries = dc.calibrationEntries.size() <= 16 ? dc.calibrationEntries.size() : 6;
            std::string entries_preview;
            for(std::size_t i = 0; i < dc.calibrationEntries.size() && i < max_calibration_entries; ++i){
                const auto& entry = dc.calibrationEntries[i];
                if(!entries_preview.empty()){
                    entries_preview += ", ";
                }
                entries_preview += std::format(
                    "settings={} ptr={}<= {}:{} oid={} body=0x{:X}",
                    entry.hasSettingsRaw ? format_std_config_value(entry.settingsRaw, entry.settingsStdConfigName)
                                         : std::string{"<none>"},
                    entry.type.display_name().empty() ? std::format("class_id={}", entry.type.class_id)
                                                      : entry.type.display_name(),
                    entry.expected.display_name().empty() ? std::format("class_id={}", entry.expected.class_id)
                                                          : entry.expected.display_name(),
                    compatibility_name(entry.compatibility),
                    entry.object_id,
                    entry.body_offset);
            }
            if(dc.calibrationEntries.size() > max_calibration_entries){
                entries_preview += ", ...";
            }
            dc_trailer_summary += std::format(" dcEntries[{}]=[{}]", dc.calibrationEntries.size(), entries_preview);
        }
    }
    std::string xmse_tail_summary;
    if(summary.hasXMSETail){
        xmse_tail_summary = std::format(
            " xmse(sumsPerCycle={} timingMode={} saturation={} turnsPerCycle=[{},{}] numPixel={})",
            summary.sumsPerCycle,
            summary.timingMode,
            summary.saturation,
            summary.turnsPerCycle0,
            summary.turnsPerCycle1,
            summary.numPixel);
    }
    std::string observed_summary;
    if(!summary.observedFields.empty()){
        constexpr std::size_t max_observed = 24;
        std::string preview;
        for(std::size_t i = 0; i < summary.observedFields.size() && i < max_observed; ++i){
            if(!preview.empty()){
                preview += ", ";
            }
            preview += summary.observedFields[i];
        }
        if(summary.observedFields.size() > max_observed){
            preview += ", ...";
        }
        observed_summary = std::format(" observed=[{}]", preview);
    }

    return std::format(
        " coverage=[{}] system={} comment={} timestamp={} base={} configInfo={}<= {}:{} oid={} body=0x{:X}",
        coverage.empty() ? "none" : coverage,
        summary.system.empty() ? "<empty>" : summary.system,
        summary.comment.empty() ? "<empty>" : summary.comment,
        timestamp_text,
        summarize_name_string(summary.baseName),
        summary.configInfoType.display_name().empty()
            ? std::format("class_id={}", summary.configInfoType.class_id)
            : summary.configInfoType.display_name(),
        summary.configInfoExpected.display_name().empty()
            ? std::format("class_id={}", summary.configInfoExpected.class_id)
            : summary.configInfoExpected.display_name(),
        compatibility_name(summary.configInfoCompatibility),
        summary.configInfoObjectId,
        summary.configInfoBodyOffset)
        + config_info_entries_summary
        + dc_trailer_summary
        + xmse_tail_summary
        + observed_summary;
}

[[nodiscard]] std::string summarize_named_value_list(
    const FTML::XMSE::ConfigurationSetEntry::NamedValueSetEntrySummary::NamedValueListSummary& list) {
    std::string summary = std::format("namedValueList[{}]", list.itemCount);
    if(list.hasEntriesCount){
        summary += std::format(" entries={}", list.entriesCountRaw);
    }

    if(!list.structuredEntries.empty()){
        std::string structured_preview;
        for(std::size_t i = 0; i < list.structuredEntries.size(); ++i){
            const auto& entry = list.structuredEntries[i];
            if(!structured_preview.empty()){
                structured_preview += ", ";
            }
            structured_preview += std::format(
                "{} tag={} ptr={}<= {}:{} oid={} body=0x{:X}",
                entry.name.empty() ? "<empty>" : entry.name,
                entry.hasValueTag ? std::format("0x{:X}", entry.valueTag) : std::string{"<none>"},
                entry.pointerType.display_name().empty()
                    ? std::format("class_id={}", entry.pointerType.class_id)
                    : entry.pointerType.display_name(),
                entry.expected.display_name().empty()
                    ? std::format("class_id={}", entry.expected.class_id)
                    : entry.expected.display_name(),
                compatibility_name(entry.compatibility),
                entry.object_id,
                entry.body_offset);
            if(entry.configuration.has_value()){
                structured_preview += summarize_configuration_summary(*entry.configuration);
            }
        }
        summary += std::format(" structured=[{}]", structured_preview);
    }

    std::string preview;
    for(std::size_t i = 0; i < list.preview.size(); ++i){
        if(!preview.empty()){
            preview += ", ";
        }
        preview += list.preview[i];
    }
    summary += std::format(" preview=[{}]", preview.empty() ? "(none)" : preview);
    return summary;
}

[[nodiscard]] std::string summarize_configuration_entry(const FTML::XMSE::ConfigurationSetEntry& entry) {
    std::string config_preview;
    if(entry.configuration.has_value()){
        config_preview += summarize_configuration_summary(*entry.configuration);
    }
    if(entry.namedValueList.has_value()){
        config_preview += " ";
        config_preview += summarize_named_value_list(*entry.namedValueList);
    }
    if(entry.namedValueSet.has_value()){
        std::string named_value_preview;
        constexpr std::size_t max_named_entries = 12;
        for(std::size_t i = 0; i < entry.namedValueSet->entries.size() && i < max_named_entries; ++i){
            const auto& nested = entry.namedValueSet->entries[i];
            if(!named_value_preview.empty()){
                named_value_preview += ", ";
            }
            named_value_preview += std::format(
                "{}#{}:{}<= {}:{} oid={} body=0x{:X}",
                nested.key.empty() ? "<unnamed>" : nested.key,
                nested.ordinal,
                nested.type.display_name().empty()
                    ? std::format("class_id={}", nested.type.class_id)
                    : nested.type.display_name(),
                nested.expected.display_name().empty()
                    ? std::format("class_id={}", nested.expected.class_id)
                    : nested.expected.display_name(),
                compatibility_name(nested.compatibility),
                nested.object_id,
                nested.body_offset);
            if(nested.configuration.has_value()){
                named_value_preview += summarize_configuration_summary(*nested.configuration);
            }
            if(nested.namedValueList.has_value()){
                named_value_preview += " ";
                named_value_preview += summarize_named_value_list(*nested.namedValueList);
            }
        }
        if(entry.namedValueSet->entries.size() > max_named_entries){
            named_value_preview += ", ...";
        }
        config_preview += std::format(
            " namedValueSet(items={}, entries={})=[{}]",
            entry.namedValueSet->itemCount,
            entry.namedValueSet->entries.size(),
            named_value_preview.empty() ? "(none)" : named_value_preview);
        if(!entry.namedValueSet->preview.empty()){
            std::string preview_text;
            for(std::size_t i = 0; i < entry.namedValueSet->preview.size(); ++i){
                if(!preview_text.empty()){
                    preview_text += ", ";
                }
                preview_text += entry.namedValueSet->preview[i];
            }
            config_preview += std::format(" preview=[{}]", preview_text);
        }
        if(!entry.namedValueSet->structuredScalars.empty()){
            std::string scalar_preview;
            for(std::size_t i = 0; i < entry.namedValueSet->structuredScalars.size(); ++i){
                const auto& scalar = entry.namedValueSet->structuredScalars[i];
                if(!scalar_preview.empty()){
                    scalar_preview += ", ";
                }
                scalar_preview += std::format(
                    "{} tag={} encoding={} type={} raw={} value={} semantic={}{}{}",
                    scalar.name.empty() ? "<unnamed>" : scalar.name,
                    scalar.hasValueTag ? std::format("0x{:X}", scalar.valueTag) : std::string{"<none>"},
                    observed_value_encoding_name(scalar.encoding),
                    scalar.rawValueType.empty() ? "<none>" : scalar.rawValueType,
                    scalar.hasTextValue ? (scalar.textValue.empty() ? std::string{"<empty>"} : scalar.textValue)
                                        : (scalar.hasSignedValue ? std::to_string(scalar.signedValue)
                                                                 : (scalar.hasUnsignedValue ? std::to_string(scalar.unsignedValue)
                                                                                           : std::string{"<none>"})),
                    scalar.valuePreview.empty() ? "<none>" : scalar.valuePreview,
                    semantic_status_name(scalar.semanticStatus),
                    scalar.semanticMeaning.empty() ? std::string{} : std::format(" meaning={}", scalar.semanticMeaning),
                    scalar.semanticNote.empty() ? std::string{} : std::format(" note={}", scalar.semanticNote));
            }
            config_preview += std::format(" scalars=[{}]", scalar_preview);
        }
    }
    return std::format(
        "{}:{}<= {}:{} oid={} body=0x{:X}{}",
        entry.key.empty() ? "<unnamed>" : entry.key,
        entry.type.display_name().empty() ? std::format("class_id={}", entry.type.class_id) : entry.type.display_name(),
        entry.expected.display_name().empty() ? std::format("class_id={}", entry.expected.class_id) : entry.expected.display_name(),
        compatibility_name(entry.compatibility),
        entry.object_id,
        entry.body_offset,
        config_preview);
}

[[nodiscard]] std::string summarize_dynamic_instance(const FTML::XMSE::DynamicObjectReadResult& result) {
    switch(result.kind){
    case FTML::XMSE::DynamicObjectKind::DCRecipe:
        if(const auto* value = result.as<FTML::XMSE::DCRecipe>(); value != nullptr){
            return std::format(
                "DCRecipe numCyclesRef={} numCyclesDark={} numCyclesSample={} analyzerRef={} analyzerSample={} rotation={}",
                value->numCyclesRef,
                value->numCyclesDark,
                value->numCyclesSample,
                value->analyzerRef,
                value->analyzerSample,
                value->rotation);
        }
        return "DCRecipe <no value>";
    case FTML::XMSE::DynamicObjectKind::DPRecipe:
        if(const auto* value = result.as<FTML::XMSE::DPRecipe>(); value != nullptr){
            return std::format(
                "DPRecipe configApp={} binning={} applyPSF={} applyTilt={} applyIDN={} modelTilt={}",
                summarize_name_string(value->configApp),
                value->binning,
                value->applyPSF ? "true" : "false",
                value->applyTilt ? "true" : "false",
                value->applyIDN ? "true" : "false",
                value->modelTilt ? "true" : "false");
        }
        return "DPRecipe <no value>";
    case FTML::XMSE::DynamicObjectKind::RawData:
        if(const auto* value = result.as<FTML::XMSE::RawData>(); value != nullptr){
            return std::format(
                "RawData numSums={} sumsPerCycle={} numPixel={} clkPeriod={} enc1Lines={} enc2Lines={} sig={} enc1={} enc2={} clk={} bm={}",
                value->numSums,
                value->sumsPerCycle,
                value->numPixel,
                value->clkPeriod,
                value->enc1Lines,
                value->enc2Lines,
                summarize_uint32_array_payload(value->sig),
                summarize_uint32_array_payload(value->enc1),
                summarize_uint32_array_payload(value->enc2),
                summarize_uint32_array_payload(value->clk),
                summarize_uint32_array_payload(value->bm));
        }
        return "RawData <no value>";
    case FTML::XMSE::DynamicObjectKind::ConfigurationSet:
        if(const auto* value = result.as<FTML::XMSE::ConfigurationSet>(); value != nullptr){
            std::string entry_preview;
            constexpr std::size_t max_entries = 3;
            for(std::size_t i = 0; i < value->entries.size() && i < max_entries; ++i){
                const auto& entry = value->entries[i];
                if(!entry_preview.empty()){
                    entry_preview += ", ";
                }
                entry_preview += summarize_configuration_entry(entry);
            }
            if(value->entries.size() > max_entries){
                entry_preview += ", ...";
            }
            if(entry_preview.empty()){
                entry_preview = value->hasLegacyRoot
                    ? std::format("legacy-root={}", summarize_configuration_entry(value->legacyRoot))
                    : "(none)";
            }
            return std::format(
                "ConfigurationSet systemID={} entries={} legacyRoot={} preview=[{}]",
                value->systemID.empty() ? "<empty>" : value->systemID,
                value->entries.size(),
                value->hasLegacyRoot ? "true" : "false",
                entry_preview);
        }
        return "ConfigurationSet <no value>";
    case FTML::XMSE::DynamicObjectKind::RawDataSet:
        if(const auto* value = result.as<FTML::XMSE::RawDataSet>(); value != nullptr){
            return std::format(
                "RawDataSet sampleID={} sysInfo={} filters(ref={}, sample={}) slots(dcRecipe={}, dpRecipe={}, ref={}, dark={}, sample={}, configSet={})",
                value->dcRawDataSet.sampleID.empty() ? "<empty>" : value->dcRawDataSet.sampleID,
                value->hasSysInfo ? summarize_configuration_entry(value->sysInfo) : std::string{"<absent>"},
                summarize_name_string(value->filterRefText),
                summarize_name_string(value->filterSampleText),
                value->hasDCRecipe ? "true" : "false",
                value->hasDPRecipe ? "true" : "false",
                value->hasRef ? "true" : "false",
                value->hasDark ? "true" : "false",
                value->hasSample ? "true" : "false",
                value->hasConfigSet ? "true" : "false");
        }
        return "RawDataSet <no value>";
    default:
        return "Unhandled";
    }
}

[[nodiscard]] std::string_view rawdata_role_name(std::size_t ordered_index) {
    switch(ordered_index){
    case 0:
        return "Ref";
    case 1:
        return "Dark";
    case 2:
        return "Sample";
    default:
        return "Unknown";
    }
}

[[nodiscard]] std::string rawdata_role_basis(std::size_t ordered_index) {
    if(ordered_index >= 3){
        return "outside RawDataSet::get Ref/Dark/Sample order";
    }
    return std::format(
        "RawDataSet::get reads slot #{} as {} in Ref->Dark->Sample order",
        ordered_index + 1,
        rawdata_role_name(ordered_index));
}

using RawDataRoleIndexByOffset = std::unordered_map<std::uint32_t, std::size_t>;

[[nodiscard]] RawDataRoleIndexByOffset build_rawdata_role_index_by_offset(std::vector<std::uint32_t> offsets) {
    std::ranges::sort(offsets);
    offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());

    RawDataRoleIndexByOffset role_index_by_offset;
    for(std::size_t i = 0; i < offsets.size(); ++i){
        role_index_by_offset.emplace(offsets[i], i);
    }
    return role_index_by_offset;
}

[[nodiscard]] std::optional<std::size_t> find_rawdata_role_index(
    const RawDataRoleIndexByOffset& role_index_by_offset,
    std::uint32_t body_offset) {
    const auto it = role_index_by_offset.find(body_offset);
    if(it == role_index_by_offset.end()){
        return std::nullopt;
    }
    return it->second;
}


[[nodiscard]] std::string summarize_business_role(
    const FTML::XMSE::DynamicObjectReadResult& result,
    const RawDataRoleIndexByOffset& rawdata_role_index_by_offset) {
    switch(result.kind){
    case FTML::XMSE::DynamicObjectKind::DCRecipe:
        return "DCRecipe";
    case FTML::XMSE::DynamicObjectKind::DPRecipe:
        return "DPRecipe";
    case FTML::XMSE::DynamicObjectKind::RawData:
        if(const auto role_index = find_rawdata_role_index(rawdata_role_index_by_offset, result.body_offset); role_index.has_value()){
            return std::format(
                "RawData:{} ({})",
                rawdata_role_name(*role_index),
                rawdata_role_basis(*role_index));
        }
        return "RawData:Unknown";
    case FTML::XMSE::DynamicObjectKind::ConfigurationSet:
        return "ConfigurationSet";
    case FTML::XMSE::DynamicObjectKind::RawDataSet:
        return "RawDataSet";
    default:
        return "Unsupported";
    }
}

[[nodiscard]] bool skip_item_for_dump(BinaryArchive& archive);

[[nodiscard]] const ArchiveUtil::ClassItem* find_class_item(const ArchiveState& state, std::uint32_t class_id) {
    const auto it = state.dictionaries.classIndexById.find(class_id);
    if(it == state.dictionaries.classIndexById.end()){
        return nullptr;
    }
    return &state.dictionaries.classList[it->second];
}

[[nodiscard]] bool is_known_array_pointer(const BinaryArchive& archive, std::uint32_t class_id, std::string_view expected_name) {
    if(expected_name == "FTML::Array2D<unsigned>" && class_id == 0x001D42B7u){
        return true;
    }
    if(expected_name == "FTML::Array<unsigned>" && class_id == 0x001D401Eu){
        return true;
    }
    if(class_id == 0){
        return false;
    }
    const auto* item = find_class_item(archive.archive().state(), class_id);
    return item != nullptr && item->name == expected_name;
}

[[nodiscard]] bool read_expected_item(BinaryArchive& archive, TypeCode::ID expected) {
    const auto header = archive.next();
    return header.hasItem && header.signTypeCode == expected && archive.get(expected, 1);
}

[[nodiscard]] bool read_expected_item_one_of(
    BinaryArchive& archive,
    std::initializer_list<TypeCode::ID> expected_types) {
    const auto header = archive.next();
    if(!header.hasItem){
        return false;
    }
    const auto matches = std::ranges::find(expected_types, header.signTypeCode) != expected_types.end();
    return matches && archive.get(header.signTypeCode, 1);
}

[[nodiscard]] bool read_formatted_uint32_array(BinaryArchive& archive, std::vector<std::uint32_t>* out_values = nullptr) {
    const auto header = archive.next();
    if(!header.hasItem || header.signTypeCode != TypeCode::ID::UInt32 || header.payloadType != TypeCode::ID::UInt32){
        return false;
    }
    if(header.payloadBytes.empty() || header.payloadBytes.size() % sizeof(std::uint32_t) != 0){
        return false;
    }

    const auto count = header.payloadBytes.size() / sizeof(std::uint32_t);
    if(!archive.get(TypeCode::ID::UInt32, count)){
        return false;
    }
    if(out_values == nullptr){
        return true;
    }

    out_values->resize(count);
    std::memcpy(out_values->data(), header.payloadBytes.data(), header.payloadBytes.size());
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

[[nodiscard]] bool skip_known_array_pointer_body(BinaryArchive& archive, std::uint32_t class_id, std::uint64_t object_id) {
    if(object_id == 0){
        return false;
    }
    if(is_known_array_pointer(archive, class_id, "FTML::Array2D<unsigned>")){
        return read_expected_item_one_of(archive, {TypeCode::ID::Int8, TypeCode::ID::UInt8, TypeCode::ID::UChar})
            && read_expected_item(archive, TypeCode::ID::Int16)
            && read_formatted_uint32_array(archive)
            && read_expected_item(archive, TypeCode::ID::EndPointer);
    }
    if(is_known_array_pointer(archive, class_id, "FTML::Array<unsigned>")){
        return read_formatted_uint32_array(archive)
            && read_expected_item(archive, TypeCode::ID::EndPointer);
    }
    return false;
}

[[nodiscard]] bool skip_until_for_dump(BinaryArchive& archive, TypeCode::ID end_marker) {
    while(true){
        const auto header = archive.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return archive.get(end_marker, 1);
        }
        if(!skip_item_for_dump(archive)){
            return false;
        }
    }
}

[[nodiscard]] std::string_view current_meta_name(const ItemHeader& header) {
    if(header.metaBytes.empty()){
        return {};
    }
    const auto* meta = reinterpret_cast<const char*>(header.metaBytes.data());
    return std::string_view(meta);
}

[[nodiscard]] bool skip_item_quiet(BinaryArchive& archive);

[[nodiscard]] bool skip_until_quiet(BinaryArchive& archive, TypeCode::ID end_marker) {
    while(true){
        const auto header = archive.next();
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode == end_marker){
            return archive.get(end_marker, 1);
        }
        if(!skip_item_quiet(archive)){
            return false;
        }
    }
}

[[nodiscard]] bool skip_item_quiet(BinaryArchive& archive) {
    const auto header = archive.next();
    if(!header.hasItem){
        return false;
    }
    switch(header.signTypeCode){
    case TypeCode::ID::BeginGroup:
        return archive.get(TypeCode::ID::BeginGroup, 1) && skip_until_quiet(archive, TypeCode::ID::EndGroup);
    case TypeCode::ID::BeginPointer:
        if(!archive.get(TypeCode::ID::BeginPointer, 1)){
            return false;
        }
        if(skip_known_array_pointer_body(archive, header.tempClassId, header.tempObjectId)){
            return true;
        }
        return skip_until_quiet(archive, TypeCode::ID::EndPointer);
    case TypeCode::ID::BeginObject:
        return archive.get(TypeCode::ID::BeginObject, 1) && skip_until_quiet(archive, TypeCode::ID::EndObject);
    case TypeCode::ID::EndGroup:
        return archive.get(TypeCode::ID::EndGroup, 1);
    case TypeCode::ID::EndPointer:
        return archive.get(TypeCode::ID::EndPointer, 1);
    case TypeCode::ID::EndObject:
        return archive.get(TypeCode::ID::EndObject, 1);
    default:
        return archive.doGet(TypeCode::ID::None);
    }
}

[[nodiscard]] bool skip_item_for_dump(BinaryArchive& archive) {
    const auto header = archive.next();
    if(!header.hasItem){
        return false;
    }
    switch(header.signTypeCode){
    case TypeCode::ID::BeginGroup:
        return archive.get(TypeCode::ID::BeginGroup, 1) && skip_until_for_dump(archive, TypeCode::ID::EndGroup);
    case TypeCode::ID::BeginPointer:
        if(!archive.get(TypeCode::ID::BeginPointer, 1)){
            return false;
        }
        if(skip_known_array_pointer_body(archive, header.tempClassId, header.tempObjectId)){
            return true;
        }
        if(header.tempObjectId != 0){
            const auto next_header = archive.next();
            if(!next_header.hasItem || next_header.signTypeCode == TypeCode::ID::BeginPointer){
                return true;
            }
            const auto saved_state = archive.archive().state();
            std::cout << std::format(
                "    [pointer body] next_type={} next_class_id={} next_object_id={}\n",
                TypeCode::name(static_cast<std::uint8_t>(next_header.signTypeCode)),
                next_header.tempClassId,
                next_header.tempObjectId);
            for(std::size_t probe_index = 0; probe_index < 8; ++probe_index){
                const auto probe = archive.next();
                if(!probe.hasItem){
                    break;
                }
                std::cout << std::format(
                    "      [probe {}] type={} class_id={} object_id={} payload_size={}\n",
                    probe_index,
                    TypeCode::name(static_cast<std::uint8_t>(probe.signTypeCode)),
                    probe.tempClassId,
                    probe.tempObjectId,
                    probe.payloadBytes.size());
                if(!archive.doGet(TypeCode::ID::None)){
                    break;
                }
            }
            archive.archive().state() = saved_state;
        }
        return skip_until_for_dump(archive, TypeCode::ID::EndPointer);
    case TypeCode::ID::BeginObject:
        return archive.get(TypeCode::ID::BeginObject, 1) && skip_until_for_dump(archive, TypeCode::ID::EndObject);
    case TypeCode::ID::EndGroup:
        return archive.get(TypeCode::ID::EndGroup, 1);
    case TypeCode::ID::EndPointer:
        return archive.get(TypeCode::ID::EndPointer, 1);
    case TypeCode::ID::EndObject:
        return archive.get(TypeCode::ID::EndObject, 1);
    default:
        return archive.doGet(TypeCode::ID::None);
    }
}

struct ReadOffsetLog {
    std::optional<std::uint64_t> base_offset;
    std::unordered_set<std::uint32_t> relative_offsets;
    std::unordered_set<std::uint64_t> absolute_offsets;
};

[[nodiscard]] std::optional<std::uint64_t> parse_hex_number(std::string_view text) {
    if(text.empty()){
        return std::nullopt;
    }
    try {
        return std::stoull(std::string(text), nullptr, 16);
    } catch(...) {
        return std::nullopt;
    }
}

[[nodiscard]] ReadOffsetLog load_read_offset_log() {
    ReadOffsetLog log{};
    const std::array candidates{
        std::filesystem::path{"OLSA\\read_offset.log"},
        std::filesystem::path{"read_offset.log"},
    };

    std::ifstream input;
    for(const auto& candidate : candidates){
        input.open(candidate);
        if(input.is_open()){
            break;
        }
    }
    if(!input.is_open()){
        return log;
    }

    std::string line;
    while(std::getline(input, line)){
        constexpr std::string_view prefix = "offset=0x";
        const auto plus = line.find('+');
        if(!line.starts_with(prefix) || plus == std::string::npos){
            continue;
        }

        const auto base = parse_hex_number(std::string_view(line).substr(prefix.size(), plus - prefix.size()));
        const auto relative = parse_hex_number(std::string_view(line).substr(plus + 1));
        if(!base.has_value() || !relative.has_value()){
            continue;
        }

        if(!log.base_offset.has_value()){
            log.base_offset = *base;
        }
        log.relative_offsets.insert(static_cast<std::uint32_t>(*relative));
        log.absolute_offsets.insert(*base + *relative);
    }
    return log;
}

struct XMSEAnalysisSnapshot {
    struct LinkedDynamicSummary {
        std::uint32_t slot_offset{};
        std::uint32_t body_offset{};
        std::uint64_t object_id{};
        OLSA::Container::ResolvedTypeInfo type;
        FTML::XMSE::DynamicObjectKind kind{FTML::XMSE::DynamicObjectKind::Unhandled};
        FTML::XMSE::DynamicReadStatus status{FTML::XMSE::DynamicReadStatus::InvalidStart};
        std::string business_role;
        std::string summary;
        std::string detail;
    };

    struct RawDataSetProvenSlots {
        bool has_sample_id_prefix_candidate{};
        std::uint32_t sample_id_prefix_offset{};
        OLSA::Container::ResolvedTypeInfo sys_info_actual;
        OLSA::Container::ResolvedTypeInfo sys_info_expected;
        FTML::SmartPointer::Compatibility sys_info_compatibility{FTML::SmartPointer::Compatibility::Unresolved};
        std::uint32_t sys_info_offset{};
        bool has_version{};
        std::uint32_t version_offset{};
        TypeCode::ID version_actual_type{TypeCode::ID::None};
        TypeCode::ID version_expected_type{TypeCode::ID::None};
        std::int32_t version_value{};
        bool has_rotation{};
        std::uint32_t rotation_offset{};
        float rotation_value{};
        bool has_analyzer_ref{};
        std::uint32_t analyzer_ref_offset{};
        float analyzer_ref_value{};
        bool has_analyzer_sample{};
        std::uint32_t analyzer_sample_offset{};
        float analyzer_sample_value{};
        bool has_tilt_flag{};
        std::uint32_t tilt_flag_offset{};
        OLSA::Container::ResolvedTypeInfo tilt_type;
        std::uint32_t tilt_offset{};
        bool has_pix_shift_ref{};
        std::uint32_t pix_shift_ref_offset{};
        float pix_shift_ref_value{};
        bool has_pix_shift_sample{};
        std::uint32_t pix_shift_sample_offset{};
        float pix_shift_sample_value{};
        bool has_filter_ref_slot{};
        std::uint32_t filter_ref_offset{};
        bool filter_ref_empty{};
        bool has_filter_sample_slot{};
        std::uint32_t filter_sample_offset{};
        bool filter_sample_empty{};
        std::uint32_t ref_slot_offset{};
        std::uint64_t ref_object_id{};
        OLSA::Container::ResolvedTypeInfo ref_type;
        std::uint32_t dark_slot_offset{};
        std::uint64_t dark_object_id{};
        OLSA::Container::ResolvedTypeInfo dark_type;
        std::uint32_t sample_slot_offset{};
        std::uint64_t sample_object_id{};
        OLSA::Container::ResolvedTypeInfo sample_type;
        std::uint32_t config_set_slot_offset{};
        std::uint64_t config_set_object_id{};
        OLSA::Container::ResolvedTypeInfo config_set_type;
    };

    struct RawDataSetTopLevelSummary {
        std::size_t node_index{static_cast<std::size_t>(-1)};
        OLSA::Container::ResolvedTypeInfo type;
        FTML::XMSE::RawDataSet value;
        std::uint32_t body_offset{};
        std::uint64_t object_id{};
        bool materialized{};
        std::string detail;
        RawDataSetProvenSlots proven_slots;
        std::vector<std::string> evidence_trace;
        std::optional<LinkedDynamicSummary> ref_summary;
        std::optional<LinkedDynamicSummary> dark_summary;
        std::optional<LinkedDynamicSummary> sample_summary;
        std::optional<LinkedDynamicSummary> config_set_summary;
    };

    ReadOffsetLog read_offset_log;
    std::vector<FTML::XMSE::DynamicObjectReadResult> results;
    RawDataRoleIndexByOffset rawdata_role_index_by_offset;
    std::optional<RawDataSetTopLevelSummary> rawdata_set;
};

void dump_dictionary_lookup_samples(
    const ArchiveState::Dictionaries& dictionaries,
    std::initializer_list<std::uint32_t> class_ids) {
    for(const auto class_id : class_ids){
        const auto it = dictionaries.classIndexById.find(class_id);
        if(it == dictionaries.classIndexById.end()){
            std::cout << std::format(
                "dictionary lookup: class_id={} hex=0x{:08X} name=<missing>\n",
                class_id,
                class_id);
            continue;
        }

        const auto& item = dictionaries.classList[it->second];
        std::string module_name = "<missing>";
        std::string module_version = "<missing>";
        if(const auto mod_it = dictionaries.moduleIndexById.find(item.module_id);
            mod_it != dictionaries.moduleIndexById.end()){
            const auto& module = dictionaries.moduleList[mod_it->second];
            module_name = module.name;
            module_version = module.version;
        }

        std::cout << std::format(
            "dictionary lookup: class_id={} hex=0x{:08X} name={} module={} module_version={}\n",
            class_id,
            class_id,
            item.name,
            module_name,
            module_version);
    }
}

[[nodiscard]] OLSA::Container::ResolvedTypeInfo resolve_type_info_from_dictionary_or_lineage(
    const ArchiveState::Dictionaries& dictionaries,
    std::uint32_t class_id) {
    OLSA::Container::ResolvedTypeInfo info{};
    if(const auto it = dictionaries.classIndexById.find(class_id); it != dictionaries.classIndexById.end()){
        const auto& item = dictionaries.classList[it->second];
        info.class_id = item.class_id;
        info.class_version = item.version;
        info.module_id = item.module_id;
        info.class_name = item.name;
        info.known = true;
        if(const auto mod_it = dictionaries.moduleIndexById.find(item.module_id);
            mod_it != dictionaries.moduleIndexById.end()){
            const auto& module = dictionaries.moduleList[mod_it->second];
            info.module_name = module.name;
            info.module_version = module.version;
        }
        return info;
    }

    if(const auto recovered = OLSA::ArchiveUtil::resolve_recovered_type_info(class_id); recovered.has_value()){
        return *recovered;
    }

    info.class_id = class_id;
    return info;
}

[[nodiscard]] XMSEAnalysisSnapshot::RawDataSetProvenSlots inspect_rawdataset_proven_slots(
    BinaryArchive& archive) {
    XMSEAnalysisSnapshot::RawDataSetProvenSlots proven{};
    const auto& dictionaries = archive.archive().state().dictionaries;
    const auto expected_sys_info = resolve_type_info_from_dictionary_or_lineage(dictionaries, 0x499602DDu);

    for(std::size_t index = 0; index <= 20; ++index){
        const auto item_offset = archive.archive().state().input.offset;
        const auto header = archive.next();
        if(!header.hasItem){
            break;
        }
        if(header.signTypeCode == TypeCode::ID::EndPointer){
            archive.get(TypeCode::ID::EndPointer, 1);
            break;
        }

        if(index == 0 && header.signTypeCode == TypeCode::ID::Char){
            proven.has_sample_id_prefix_candidate = true;
            proven.sample_id_prefix_offset = item_offset;
        }
        if(index == 1 && header.signTypeCode == TypeCode::ID::BeginPointer){
            proven.sys_info_offset = item_offset;
            proven.sys_info_actual = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
            proven.sys_info_expected = expected_sys_info;
            proven.sys_info_compatibility = OLSA::ArchiveUtil::classify_type_compatibility(
                proven.sys_info_actual,
                proven.sys_info_expected);
        }
        if(index == 2 && header.signTypeCode == TypeCode::ID::Int8 && header.payloadBytes.size() == 1){
            proven.has_version = true;
            proven.version_offset = item_offset;
            proven.version_actual_type = header.signTypeCode;
            proven.version_expected_type = TypeCode::ID::Int16;
            proven.version_value = static_cast<std::int32_t>(
                read_little_endian<std::int8_t>(as_bytes(header.payloadBytes)));
        }
        if(index == 5 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.rotation_offset = item_offset;
            proven.has_rotation = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 6 && header.signTypeCode == TypeCode::ID::Float && header.payloadBytes.size() == sizeof(float)){
            proven.rotation_value = read_little_endian<float>(as_bytes(header.payloadBytes));
        }
        if(index == 7 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.analyzer_ref_offset = item_offset;
            proven.has_analyzer_ref = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 8 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.analyzer_sample_offset = item_offset;
            proven.has_analyzer_sample = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 9 && header.signTypeCode == TypeCode::ID::Float && header.payloadBytes.size() == sizeof(float)){
            proven.analyzer_sample_value = read_little_endian<float>(as_bytes(header.payloadBytes));
        }
        if(index == 10 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.tilt_flag_offset = item_offset;
            proven.has_tilt_flag = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 11 && header.signTypeCode == TypeCode::ID::BeginObject){
            proven.tilt_offset = item_offset;
            proven.tilt_type = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
        }
        if(index == 12 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.pix_shift_ref_offset = item_offset;
            proven.has_pix_shift_ref = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 13 && header.signTypeCode == TypeCode::ID::Bool && header.payloadBytes.size() == 1){
            proven.pix_shift_sample_offset = item_offset;
            proven.has_pix_shift_sample = read_little_endian<std::uint8_t>(as_bytes(header.payloadBytes)) != 0;
        }
        if(index == 14 && header.signTypeCode == TypeCode::ID::Float && header.payloadBytes.size() == sizeof(float)){
            proven.pix_shift_sample_value = read_little_endian<float>(as_bytes(header.payloadBytes));
        }
        if(index == 15
            && (header.payloadType == TypeCode::ID::Char || header.payloadType == TypeCode::ID::UChar)){
            proven.has_filter_ref_slot = true;
            proven.filter_ref_offset = item_offset;
            proven.filter_ref_empty = header.payloadBytes.empty();
        }
        if(index == 16
            && (header.payloadType == TypeCode::ID::Char || header.payloadType == TypeCode::ID::UChar)){
            proven.has_filter_sample_slot = true;
            proven.filter_sample_offset = item_offset;
            proven.filter_sample_empty = header.payloadBytes.empty();
        }
        if(index == 17 && header.signTypeCode == TypeCode::ID::BeginPointer){
            proven.ref_slot_offset = item_offset;
            proven.ref_object_id = header.tempObjectId;
            proven.ref_type = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
        }
        if(index == 18 && header.signTypeCode == TypeCode::ID::BeginPointer){
            proven.dark_slot_offset = item_offset;
            proven.dark_object_id = header.tempObjectId;
            proven.dark_type = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
        }
        if(index == 19 && header.signTypeCode == TypeCode::ID::BeginPointer){
            proven.sample_slot_offset = item_offset;
            proven.sample_object_id = header.tempObjectId;
            proven.sample_type = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
        }
        if(index == 20 && header.signTypeCode == TypeCode::ID::BeginPointer){
            proven.config_set_slot_offset = item_offset;
            proven.config_set_object_id = header.tempObjectId;
            proven.config_set_type = resolve_type_info_from_dictionary_or_lineage(dictionaries, header.tempClassId);
        }

        if(!skip_item_quiet(archive)){
            break;
        }
    }

    return proven;
}

[[nodiscard]] std::vector<std::string> collect_current_item_sequence_trace(
    BinaryArchive& archive,
    TypeCode::ID end_marker,
    std::size_t max_items) {
    std::vector<std::string> lines;
    for(std::size_t index = 0; index < max_items; ++index){
        const auto item_offset = archive.archive().state().input.offset;
        const auto header = archive.next();
        if(!header.hasItem){
            lines.push_back(std::format("  [{}] offset=0x{:X} <eof>", index, item_offset));
            break;
        }
        if(header.signTypeCode == end_marker){
            lines.push_back(std::format(
                "  [{}] offset=0x{:X} type={} <end-marker>",
                index,
                item_offset,
                TypeCode::name(static_cast<std::uint8_t>(header.signTypeCode))));
            archive.get(end_marker, 1);
            break;
        }

        const auto meta = current_meta_name(header);
        const auto consumed = skip_item_quiet(archive);
        const auto end_offset = archive.archive().state().input.offset;
        lines.push_back(std::format(
            "  [{}] offset=0x{:X} type={} meta={} class_id={} object_id={} payload_size={} end=0x{:X} consumed={}",
            index,
            item_offset,
            TypeCode::name(static_cast<std::uint8_t>(header.signTypeCode)),
            meta.empty() ? "<none>" : std::string(meta),
            header.tempClassId,
            header.tempObjectId,
            header.payloadBytes.size(),
            end_offset,
            consumed ? "true" : "false"));
        if(!consumed){
            break;
        }
    }
    return lines;
}

[[nodiscard]] std::optional<XMSEAnalysisSnapshot::RawDataSetTopLevelSummary> try_read_preloaded_rawdataset_summary(
    BinaryArchive& archive) {
    const auto& state = archive.archive().state();
    const auto& resolved = state.parsing.resolvedType;
    if(!resolved.has_value() || resolved->class_name != "FTML::XMSE::RawDataSet"){
        return std::nullopt;
    }

    XMSEAnalysisSnapshot::RawDataSetTopLevelSummary summary{};
    summary.type = *resolved;
    summary.body_offset = state.input.offset;
    summary.object_id = state.parsing.itemHeader.tempObjectId;

    if(!summary.value.get(archive)){
        summary.detail = "failed while reading preloaded top-level RawDataSet body";
        return summary;
    }
    if(!archive.get(TypeCode::ID::EndPointer, 1)){
        summary.detail = "missing EndPointer after preloaded top-level RawDataSet";
        return summary;
    }

    summary.materialized = true;
    summary.detail = "materialized preloaded top-level XMSE::RawDataSet";
    return summary;
}

[[nodiscard]] std::optional<XMSEAnalysisSnapshot::RawDataSetTopLevelSummary> try_read_first_rawdataset_summary(
    BinaryArchive& archive) {
    while(true){
        const auto header = archive.next();
        if(!header.hasItem){
            return std::nullopt;
        }
        if(header.signTypeCode != TypeCode::ID::BeginPointer
            && header.signTypeCode != TypeCode::ID::BeginObject){
            if(!archive.doGet(TypeCode::ID::None)){
                return std::nullopt;
            }
            continue;
        }

        FTML::XMSE::DynamicObjectReadResult result{};
        const auto ok = FTML::XMSE::read_dynamic(archive, result);
        if(result.type.class_name != "FTML::XMSE::RawDataSet"){
            if(!ok){
                continue;
            }
            continue;
        }

        XMSEAnalysisSnapshot::RawDataSetTopLevelSummary summary{};
        summary.type = result.type;
        summary.body_offset = result.body_offset;
        summary.object_id = result.object_id;
        summary.detail = result.detail;
        if(ok && result.kind == FTML::XMSE::DynamicObjectKind::RawDataSet){
            if(const auto* value = result.as<FTML::XMSE::RawDataSet>(); value != nullptr){
                summary.value = *value;
                summary.materialized = true;
            }
        }
        return summary;
    }
}

[[nodiscard]] BinaryArchive make_node_archive(
    BinaryFileReader& reader,
    std::size_t node_index,
    const ArchiveState::Dictionaries& dictionaries) {
    BinaryArchive archive;
    archive.reset(reader.read_data_block(node_index));
    archive.read_first_section();
    archive.archive().state().dictionaries = dictionaries;
    return archive;
}

[[nodiscard]] std::optional<XMSEAnalysisSnapshot::RawDataSetTopLevelSummary> find_first_rawdataset_summary_across_nodes(
    BinaryFileReader& reader,
    const ArchiveState::Dictionaries& dictionaries) {
    const auto& nodes = reader.get_nodes();
    for(std::size_t i = 0; i < nodes.size(); ++i){
        BinaryArchive candidate = make_node_archive(reader, i, dictionaries);
        if(auto summary = try_read_preloaded_rawdataset_summary(candidate); summary.has_value()){
            BinaryArchive traced = make_node_archive(reader, i, dictionaries);
            summary->proven_slots = inspect_rawdataset_proven_slots(traced);
            BinaryArchive traced_items = make_node_archive(reader, i, dictionaries);
            summary->evidence_trace = collect_current_item_sequence_trace(traced_items, TypeCode::ID::EndPointer, 24);
            summary->node_index = i;
            return summary;
        }
        candidate.archive().state().parsing.itemHeader = {};
        auto summary = try_read_first_rawdataset_summary(candidate);
        if(summary.has_value()){
            summary->node_index = i;
            return summary;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<XMSEAnalysisSnapshot::LinkedDynamicSummary> find_linked_dynamic_summary(
    std::uint32_t slot_offset,
    std::uint64_t object_id,
    const OLSA::Container::ResolvedTypeInfo& type,
    const std::vector<FTML::XMSE::DynamicObjectReadResult>& results,
    const RawDataRoleIndexByOffset& rawdata_role_index_by_offset,
    std::string_view fallback_business_role = {}) {
    if(slot_offset == 0){
        return std::nullopt;
    }

    constexpr std::uint32_t pointer_body_delta = 0xEu;
    const auto expected_body_offset = slot_offset + pointer_body_delta;
    const FTML::XMSE::DynamicObjectReadResult* matched = nullptr;

    for(const auto& result : results){
        if(result.object_id == object_id && object_id != 0){
            matched = &result;
            break;
        }
    }
    if(matched == nullptr){
        for(const auto& result : results){
            if(result.body_offset == expected_body_offset){
                matched = &result;
                break;
            }
        }
    }
    if(matched == nullptr){
        return std::nullopt;
    }

    XMSEAnalysisSnapshot::LinkedDynamicSummary linked{};
    linked.slot_offset = slot_offset;
    linked.body_offset = matched->body_offset;
    linked.object_id = matched->object_id;
    linked.type = type.class_id != 0 ? type : matched->type;
    linked.kind = matched->kind;
    linked.status = matched->status;
    linked.business_role = summarize_business_role(*matched, rawdata_role_index_by_offset);
    if(linked.business_role == "RawData:Unknown" && !fallback_business_role.empty()){
        linked.business_role = std::string(fallback_business_role);
    }
    linked.summary = summarize_dynamic_instance(*matched);
    linked.detail = matched->detail;
    return linked;
}

[[nodiscard]] std::optional<std::string_view> infer_top_level_rawdata_role(
    const XMSEAnalysisSnapshot& snapshot,
    std::uint32_t body_offset) {
    if(!snapshot.rawdata_set.has_value()){
        return std::nullopt;
    }
    const auto& top = *snapshot.rawdata_set;
    if(top.ref_summary.has_value() && top.ref_summary->body_offset == body_offset){
        return "RawData:Ref";
    }
    if(top.dark_summary.has_value() && top.dark_summary->body_offset == body_offset){
        return "RawData:Dark";
    }
    if(top.sample_summary.has_value() && top.sample_summary->body_offset == body_offset){
        return "RawData:Sample";
    }
    return std::nullopt;
}

[[nodiscard]] XMSEAnalysisSnapshot build_xmse_analysis_snapshot(BinaryArchive& archive, BinaryFileReader& reader) {
    XMSEAnalysisSnapshot snapshot{};
    snapshot.read_offset_log = load_read_offset_log();
    snapshot.results = FTML::XMSE::collect_recognized_dynamic_objects(archive);
    std::ranges::sort(snapshot.results, [](const auto& lhs, const auto& rhs) {
        return lhs.body_offset < rhs.body_offset;
    });

    std::vector<std::uint32_t> rawdata_offsets;
    for(const auto& result : snapshot.results){
        if(result.kind == FTML::XMSE::DynamicObjectKind::RawData
            && snapshot.read_offset_log.relative_offsets.contains(result.body_offset)){
            rawdata_offsets.push_back(result.body_offset);
        }
    }
    snapshot.rawdata_role_index_by_offset = build_rawdata_role_index_by_offset(std::move(rawdata_offsets));
    snapshot.rawdata_set = find_first_rawdataset_summary_across_nodes(reader, archive.archive().state().dictionaries);
    if(snapshot.rawdata_set.has_value()){
        auto& top = *snapshot.rawdata_set;
        top.ref_summary = find_linked_dynamic_summary(
            top.proven_slots.ref_slot_offset,
            top.proven_slots.ref_object_id,
            top.proven_slots.ref_type,
            snapshot.results,
            snapshot.rawdata_role_index_by_offset,
            "RawData:Ref");
        top.dark_summary = find_linked_dynamic_summary(
            top.proven_slots.dark_slot_offset,
            top.proven_slots.dark_object_id,
            top.proven_slots.dark_type,
            snapshot.results,
            snapshot.rawdata_role_index_by_offset,
            "RawData:Dark");
        top.sample_summary = find_linked_dynamic_summary(
            top.proven_slots.sample_slot_offset,
            top.proven_slots.sample_object_id,
            top.proven_slots.sample_type,
            snapshot.results,
            snapshot.rawdata_role_index_by_offset,
            "RawData:Sample");
        top.config_set_summary = find_linked_dynamic_summary(
            top.proven_slots.config_set_slot_offset,
            top.proven_slots.config_set_object_id,
            top.proven_slots.config_set_type,
            snapshot.results,
            snapshot.rawdata_role_index_by_offset);
    }
    return snapshot;
}

void dump_dynamic_type_samples(BinaryArchive& archive, std::uint32_t expected_class_id, std::size_t max_samples) {
    std::set<std::uint32_t> seen_class_ids;
    std::size_t sample_count = 0;

    while(true){
        const auto header = archive.next();
        if(!header.hasItem){
            break;
        }

        bool recognized = false;
        FTML::SmartPointer::ExtractResult result{};
        if(header.signTypeCode == TypeCode::ID::BeginPointer){
            recognized = FTML::SmartPointer::extract(archive, expected_class_id, result);
        } else if(header.signTypeCode == TypeCode::ID::BeginObject) {
            recognized = archive.getObject();
        } else {
            archive.doGet(TypeCode::ID::None);
        }

        if(!recognized || !result.recognized()){
            continue;
        }

        const auto& info = result.actual;
        if(!seen_class_ids.insert(info.class_id).second){
            continue;
        }

        std::cout << std::format(
            "type sample: kind={} class_id={} type={} module={} module_version={} known={} from_tag={} expected_type={} matches_expected={} compatibility={}\n",
            info.is_pointer ? "pointer" : "object",
            info.class_id,
            info.display_name(),
            info.module_name,
            info.module_version,
            info.known ? "true" : "false",
            info.from_tag ? "true" : "false",
            summarize_resolved_type(result.expected),
            result.matches_expected() ? "true" : "false",
            compatibility_name(result.compatibility));
        sample_count += 1;
        if(sample_count >= max_samples){
            break;
        }
    }
}

void dump_targeted_xmse_samples(const XMSEAnalysisSnapshot& snapshot, std::size_t max_samples) {
    std::set<std::uint32_t> seen_class_ids;
    std::size_t sample_count = 0;

    for(const auto& result : snapshot.results){
        if(!result.handled()){
            continue;
        }

        if(!seen_class_ids.insert(result.type.class_id).second){
            continue;
        }

        const std::string summary = summarize_dynamic_instance(result);

        std::string business_role = summarize_business_role(result, snapshot.rawdata_role_index_by_offset);
        if(business_role == "RawData:Unknown"){
            if(const auto inferred = infer_top_level_rawdata_role(snapshot, result.body_offset); inferred.has_value()){
                business_role = std::string(*inferred);
            }
        }
        std::cout << std::format(
            "targeted read: role={} class_id={} type={} summary={}\n",
            business_role,
            result.type.class_id,
            result.type.display_name(),
            summary);
        sample_count += 1;
        if(sample_count >= max_samples){
            break;
        }
    }
}

void dump_current_capability_boundaries(const XMSEAnalysisSnapshot& snapshot) {
    std::size_t handled_dc_recipe = 0;
    std::size_t handled_dp_recipe = 0;
    std::size_t handled_raw_data = 0;
    std::size_t handled_configuration_set = 0;
    std::size_t handled_raw_data_set = 0;
    std::size_t parse_failed = 0;
    std::size_t no_materialized = 0;
    std::size_t unsupported = 0;

    for(const auto& result : snapshot.results){
        switch(result.status){
        case FTML::XMSE::DynamicReadStatus::Handled:
            switch(result.kind){
            case FTML::XMSE::DynamicObjectKind::DCRecipe:
                handled_dc_recipe += 1;
                break;
            case FTML::XMSE::DynamicObjectKind::DPRecipe:
                handled_dp_recipe += 1;
                break;
            case FTML::XMSE::DynamicObjectKind::RawData:
                handled_raw_data += 1;
                break;
            case FTML::XMSE::DynamicObjectKind::ConfigurationSet:
                handled_configuration_set += 1;
                break;
            case FTML::XMSE::DynamicObjectKind::RawDataSet:
                handled_raw_data_set += 1;
                break;
            default:
                break;
            }
            break;
        case FTML::XMSE::DynamicReadStatus::ParseFailed:
            parse_failed += 1;
            break;
        case FTML::XMSE::DynamicReadStatus::NoMaterializedFields:
            no_materialized += 1;
            break;
        case FTML::XMSE::DynamicReadStatus::UnsupportedType:
            unsupported += 1;
            break;
        default:
            break;
        }
    }

    std::cout << "current capability boundary:\n";
    std::cout << std::format(
        "  supported_materialization=FTML::XMSE::DCRecipe/DPRecipe/RawData/ConfigurationSet/RawDataSet handled_counts={{DCRecipe:{}, DPRecipe:{}, RawData:{}, ConfigurationSet:{}, RawDataSet:{}}}\n",
        handled_dc_recipe,
        handled_dp_recipe,
        handled_raw_data,
        handled_configuration_set,
        handled_raw_data_set);
    std::cout << std::format(
        "  supported_identification=dynamic pointers/objects with dictionary or recovered lineage total_recognized_xmse={} parse_failed={} no_materialized={} unsupported={}\n",
        snapshot.results.size(),
        parse_failed,
        no_materialized,
        unsupported);
    if(snapshot.rawdata_set.has_value()){
        const auto& summary = *snapshot.rawdata_set;
        std::cout << std::format(
            "  rawdataset_top_level: node={} type={} body_offset=0x{:X} object_id={} materialized={} detail={}\n",
            summary.node_index,
            summary.type.display_name(),
            summary.body_offset,
            summary.object_id,
            summary.materialized ? "true" : "false",
            summary.detail.empty() ? "<none>" : summary.detail);
        if(!summary.evidence_trace.empty()){
            if(summary.proven_slots.has_sample_id_prefix_candidate
                || summary.proven_slots.sys_info_actual.class_id != 0
                || summary.proven_slots.has_version
                || summary.proven_slots.tilt_type.class_id != 0){
                std::cout << std::format(
                    "  rawdataset_proven_slots: sampleID={} sampleID_prefix_probe={} sysInfo@0x{:X}={}<= {}:{} version@0x{:X}={} value={} expected_read={} rotation@0x{:X}=({},{:.6f}) analyzerRef@0x{:X}=({},{:.6f}) analyzerSample@0x{:X}=({},{:.6f}) tiltFlag@0x{:X}={} tilt@0x{:X}={} pixShiftRef@0x{:X}=({},{:.6f}) pixShiftSample@0x{:X}=({},{:.6f}) filterRef@0x{:X}={} filterSample@0x{:X}={}\n",
                    summary.materialized
                        ? (summary.value.dcRawDataSet.sampleID.empty() ? std::string{"<empty>"} : summary.value.dcRawDataSet.sampleID)
                        : std::string{"<unmaterialized>"},
                    summary.proven_slots.has_sample_id_prefix_candidate
                        ? std::format("char@0x{:X}", summary.proven_slots.sample_id_prefix_offset)
                        : "<absent>",
                    summary.proven_slots.sys_info_offset,
                    summary.proven_slots.sys_info_actual.display_name().empty()
                        ? std::format("class_id={}", summary.proven_slots.sys_info_actual.class_id)
                        : summary.proven_slots.sys_info_actual.display_name(),
                    summary.proven_slots.sys_info_expected.display_name().empty()
                        ? std::format("class_id={}", summary.proven_slots.sys_info_expected.class_id)
                        : summary.proven_slots.sys_info_expected.display_name(),
                    compatibility_name(summary.proven_slots.sys_info_compatibility),
                    summary.proven_slots.version_offset,
                    summary.proven_slots.has_version
                        ? TypeCode::name(static_cast<std::uint8_t>(summary.proven_slots.version_actual_type))
                        : "<absent>",
                    summary.proven_slots.version_value,
                    summary.proven_slots.has_version
                        ? TypeCode::name(static_cast<std::uint8_t>(summary.proven_slots.version_expected_type))
                        : "<none>",
                    summary.proven_slots.rotation_offset,
                    summary.proven_slots.has_rotation ? "true" : "false",
                    summary.proven_slots.rotation_value,
                    summary.proven_slots.analyzer_ref_offset,
                    summary.proven_slots.has_analyzer_ref ? "true" : "false",
                    summary.proven_slots.analyzer_ref_value,
                    summary.proven_slots.analyzer_sample_offset,
                    summary.proven_slots.has_analyzer_sample ? "true" : "false",
                    summary.proven_slots.analyzer_sample_value,
                    summary.proven_slots.tilt_flag_offset,
                    summary.proven_slots.has_tilt_flag ? "true" : "false",
                    summary.proven_slots.tilt_offset,
                    summary.proven_slots.tilt_type.display_name().empty()
                        ? std::format("class_id={}", summary.proven_slots.tilt_type.class_id)
                        : summary.proven_slots.tilt_type.display_name(),
                    summary.proven_slots.pix_shift_ref_offset,
                    summary.proven_slots.has_pix_shift_ref ? "true" : "false",
                    summary.proven_slots.pix_shift_ref_value,
                    summary.proven_slots.pix_shift_sample_offset,
                    summary.proven_slots.has_pix_shift_sample ? "true" : "false",
                    summary.proven_slots.pix_shift_sample_value,
                    summary.proven_slots.filter_ref_offset,
                    summary.proven_slots.has_filter_ref_slot
                        ? (summary.proven_slots.filter_ref_empty ? "<empty>" : "<non-empty>")
                        : "<absent>",
                    summary.proven_slots.filter_sample_offset,
                    summary.proven_slots.has_filter_sample_slot
                        ? (summary.proven_slots.filter_sample_empty ? "<empty>" : "<non-empty>")
                        : "<absent>");
            }
            std::cout << "  rawdataset_top_level_trace:\n";
            for(const auto& line : summary.evidence_trace){
                std::cout << line << "\n";
            }
        }
        if(summary.materialized){
            std::cout << "  rawdataset_front:\n";
            std::cout << std::format(
                "    sampleID={}\n",
                summary.value.dcRawDataSet.sampleID.empty() ? "<empty>" : summary.value.dcRawDataSet.sampleID);
            std::cout << std::format("    tilt={}\n", summarize_tilt_summary(summary.value));
            if(summary.value.hasSysInfo){
                dump_node3_sysinfo_block(summary.value.sysInfo, "    ");
            } else {
                std::cout << "    sysInfo=<absent>\n";
            }
            std::cout << std::format(
                "  rawdataset_filters: ref={} sample={}\n",
                summarize_name_string(summary.value.filterRefText),
                summarize_name_string(summary.value.filterSampleText));
            std::cout << std::format(
                "  rawdataset_slots: dcRecipe={} dpRecipe={} ref={} dark={} sample={} configSet={}\n",
                summary.value.hasDCRecipe ? "true" : "false",
                summary.value.hasDPRecipe ? "true" : "false",
                summary.value.hasRef ? "true" : "false",
                summary.value.hasDark ? "true" : "false",
                summary.value.hasSample ? "true" : "false",
                summary.value.hasConfigSet ? "true" : "false");
        }
        const auto dump_linked = [&](std::string_view slot_name,
                                     const std::optional<XMSEAnalysisSnapshot::LinkedDynamicSummary>& linked) {
            if(!linked.has_value()){
                std::cout << std::format("  rawdataset_linked_{}: <unresolved>\n", slot_name);
                return;
            }
            std::cout << std::format(
                "  rawdataset_linked_{}: slot=0x{:X} body=0x{:X} object_id={} role={} type={} status={} summary={}\n",
                slot_name,
                linked->slot_offset,
                linked->body_offset,
                linked->object_id,
                linked->business_role,
                linked->type.display_name().empty() ? std::format("class_id={}", linked->type.class_id) : linked->type.display_name(),
                dynamic_read_status_name(linked->status),
                linked->summary);
        };
        dump_linked("ref", summary.ref_summary);
        dump_linked("dark", summary.dark_summary);
        dump_linked("sample", summary.sample_summary);
        dump_linked("configSet", summary.config_set_summary);
    } else {
        std::cout << "  rawdataset_top_level: none\n";
    }
    std::cout << "  rawdata_prefix_fields=numSums/sumsPerCycle/timingMode/numPixel/turnsPerCycle0/turnsPerCycle1/numBM/firstSum/firstAcqSum/pixelRange/clkPeriod/enc1Lines/enc2Lines\n";
    std::cout << "  rawdata_tail_arrays=sig/enc1/enc2/clk/bm only when pointer bodies resolve as FTML::Array2D<unsigned> or FTML::Array<unsigned>\n";
    std::cout << "  current_limitations=no generic body reader for non-XMSE targeted types; no generic container materialization beyond known uint32 arrays; cross-node/cross-block object realization is not implemented\n";
}

void dump_unsupported_xmse_instances(const XMSEAnalysisSnapshot& snapshot) {
    std::size_t count = 0;
    std::cout << "unsupported xmse instances:\n";
    for(const auto& result : snapshot.results){
        if(result.status != FTML::XMSE::DynamicReadStatus::UnsupportedType){
            continue;
        }
        std::cout << std::format(
            "  type={} offset=0x{:X} matched_log={} object_id={} tag={} detail={}\n",
            result.type.display_name(),
            result.body_offset,
            snapshot.read_offset_log.relative_offsets.contains(result.body_offset) ? "true" : "false",
            result.object_id,
            result.tag,
            result.detail);
        count += 1;
    }
    if(count == 0){
        std::cout << "  none\n";
    }
}

void dump_test_instance_parse_report(const XMSEAnalysisSnapshot& snapshot) {
    std::cout << "test instance parse report:\n";
    for(const auto& result : snapshot.results){
        if(!snapshot.read_offset_log.relative_offsets.contains(result.body_offset)){
            continue;
        }

        std::string business_role = summarize_business_role(result, snapshot.rawdata_role_index_by_offset);
        if(business_role == "RawData:Unknown"){
            if(const auto inferred = infer_top_level_rawdata_role(snapshot, result.body_offset); inferred.has_value()){
                business_role = std::string(*inferred);
            }
        }

        std::cout << std::format(
            "  role={} offset=0x{:X} type={} status={} object_id={} tag={} detail={} summary={}\n",
            business_role,
            result.body_offset,
            result.type.display_name(),
            dynamic_read_status_name(result.status),
            result.object_id,
            result.tag,
            result.detail,
            summarize_dynamic_instance(result));
    }
}

void dump_targeted_rawdata_body_summaries(
    const XMSEAnalysisSnapshot& snapshot,
    std::initializer_list<std::uint32_t> body_offsets) {
    std::set<std::uint32_t> targets(body_offsets.begin(), body_offsets.end());
    std::set<std::uint32_t> printed;

    for(const auto& result : snapshot.results){
        if(result.type.class_name != "FTML::XMSE::RawData" || !targets.contains(result.body_offset)){
            continue;
        }

        const auto role_index = find_rawdata_role_index(snapshot.rawdata_role_index_by_offset, result.body_offset);
        const auto inferred_role = infer_top_level_rawdata_role(snapshot, result.body_offset);
        const auto role_name = role_index.has_value()
            ? std::string(rawdata_role_name(*role_index))
            : (inferred_role.has_value() ? std::string(*inferred_role) : std::string{"Unknown"});
        const auto role_basis = role_index.has_value()
            ? rawdata_role_basis(*role_index)
            : (inferred_role.has_value() ? std::string{"node[3] top-level linked slot"} : std::string{"outside RawDataSet::get Ref/Dark/Sample order"});

        std::string summary = std::format(
            "role={} role_basis=\"{}\" status={} matched_log={} object_id={} tag={} detail={}",
            role_name,
            role_basis,
            dynamic_read_status_name(result.status),
            snapshot.read_offset_log.relative_offsets.contains(result.body_offset) ? "true" : "false",
            result.object_id,
            result.tag,
            result.detail);

        if(const auto* value = result.as<FTML::XMSE::RawData>(); value != nullptr){
            summary += std::format(
                " numSums={} numPixel={} clkPeriod={} enc1Lines={} enc2Lines={} sig={} enc1={} enc2={} clk={} bm={}",
                value->numSums,
                value->numPixel,
                value->clkPeriod,
                value->enc1Lines,
                value->enc2Lines,
                summarize_uint32_array_payload(value->sig),
                summarize_uint32_array_payload(value->enc1),
                summarize_uint32_array_payload(value->enc2),
                summarize_uint32_array_payload(value->clk),
                summarize_uint32_array_payload(value->bm));
        }

        std::cout << std::format(
            "targeted rawdata body: offset=0x{:X} summary={}\n",
            result.body_offset,
            summary);
        printed.insert(result.body_offset);
    }

    for(const auto target : targets){
        if(printed.contains(target)){
            continue;
        }
        std::cout << std::format(
            "targeted rawdata body: offset=0x{:X} summary=missing\n",
            target);
    }
}

void dump_targeted_xmse_trace_samples(const XMSEAnalysisSnapshot& snapshot, std::size_t max_samples) {
    std::set<std::string> seen_entries;
    std::size_t sample_count = 0;

    for(const auto& result : snapshot.results){
        const auto dedupe_key = std::format(
            "{}:{}:{}",
            result.type.class_id,
            result.started_from_pointer ? "pointer" : "object",
            result.object_id);
        if(!seen_entries.insert(dedupe_key).second){
            continue;
        }

        std::cout << std::format(
            "trace: class_id={} type={} entry={} body_offset=0x{:X} matched_log={} object_id={} tag={} status={} detail={} observed=[{}]\n",
            result.type.class_id,
            result.type.display_name(),
            result.started_from_pointer ? "BeginPointer" : "BeginObject",
            result.body_offset,
            snapshot.read_offset_log.relative_offsets.contains(result.body_offset) ? "true" : "false",
            result.object_id,
            result.tag,
            dynamic_read_status_name(result.status),
            result.detail,
            summarize_observed_fields(result));
        sample_count += 1;
        if(sample_count >= max_samples){
            break;
        }
    }
}

void dump_targeted_xmse_pairing_samples(const XMSEAnalysisSnapshot& snapshot, std::size_t max_samples) {
    std::size_t sample_count = 0;

    std::vector<const FTML::XMSE::DynamicObjectReadResult*> pointers;
    std::vector<const FTML::XMSE::DynamicObjectReadResult*> objects;
    for(const auto& result : snapshot.results){
        if(result.started_from_pointer){
            pointers.push_back(&result);
        } else {
            objects.push_back(&result);
        }
    }

    std::set<std::string> seen_pointers;
    for(const auto* pointer : pointers){
        const auto dedupe_key = std::format("{}:{}", pointer->type.class_id, pointer->object_id);
        if(!seen_pointers.insert(dedupe_key).second){
            continue;
        }

        std::size_t same_id_matches = 0;
        std::size_t same_type_matches = 0;
        for(const auto* object : objects){
            if(object->type.class_id == pointer->type.class_id){
                same_type_matches += 1;
                if(pointer->object_id != 0 && object->object_id == pointer->object_id){
                    same_id_matches += 1;
                }
            }
        }

        std::cout << std::format(
            "pairing: pointer_type={} class_id={} body_offset=0x{:X} matched_log={} object_id={} pointer_status={} same_type_objects={} same_id_objects={}\n",
            pointer->type.display_name(),
            pointer->type.class_id,
            pointer->body_offset,
            snapshot.read_offset_log.relative_offsets.contains(pointer->body_offset) ? "true" : "false",
            pointer->object_id,
            dynamic_read_status_name(pointer->status),
            same_type_matches,
            same_id_matches);
        sample_count += 1;
        if(sample_count >= max_samples){
            break;
        }
    }

    if(sample_count == 0){
        std::cout << "pairing: no targeted XMSE pointer entries found\n";
    }
}

void dump_unified_testdat_report(const XMSEAnalysisSnapshot& snapshot) {
    std::cout << "unified test.dat report:\n";
    dump_current_capability_boundaries(snapshot);
    dump_unsupported_xmse_instances(snapshot);
    dump_test_instance_parse_report(snapshot);
    std::cout << "targeted XMSE reads:\n";
    dump_targeted_xmse_samples(snapshot, 6);
    std::cout << "targeted XMSE trace:\n";
    dump_targeted_xmse_trace_samples(snapshot, 12);
    std::cout << "targeted XMSE pairing:\n";
    dump_targeted_xmse_pairing_samples(snapshot, 12);
    std::cout << "targeted rawdata bodies:\n";
    dump_targeted_rawdata_body_summaries(snapshot, {0x80Eu, 0x88Eu, 0xCCEu});
}

void dump_rawdata_body_sequences(BinaryArchive& archive, std::initializer_list<std::uint32_t> body_offsets) {
    std::set<std::uint32_t> targets(body_offsets.begin(), body_offsets.end());

    while(true){
        const auto header = archive.next();
        if(!header.hasItem){
            break;
        }

        if(header.signTypeCode != TypeCode::ID::BeginPointer){
            archive.doGet(TypeCode::ID::None);
            continue;
        }

        FTML::SmartPointer::ExtractResult extracted{};
        if(!FTML::SmartPointer::extract(archive, 0, extracted) || !extracted.recognized()){
            continue;
        }
        const auto body_offset = archive.archive().state().input.offset;
        if(extracted.actual.class_name != "FTML::XMSE::RawData" || !targets.contains(body_offset)){
            if(!skip_until_for_dump(archive, TypeCode::ID::EndPointer)){
                break;
            }
            continue;
        }

        std::cout << std::format("rawdata body dump: offset=0x{:X}\n", body_offset);
        std::size_t index = 0;
        while(true){
            const auto item = archive.next();
            if(!item.hasItem){
                break;
            }
            if(item.signTypeCode == TypeCode::ID::EndPointer){
                if(!archive.get(TypeCode::ID::EndPointer, 1)){
                    break;
                }
                std::cout << "  [end]\n";
                break;
            }

            std::cout << std::format(
                "  [{}] type={} meta={} class_id={} object_id={} payload_size={}\n",
                index,
                TypeCode::name(static_cast<std::uint8_t>(item.signTypeCode)),
                item.metaBytes.empty() ? "<none>" : reinterpret_cast<const char*>(item.metaBytes.data()),
                item.tempClassId,
                item.tempObjectId,
                item.payloadBytes.size());
            index += 1;
            if(!skip_item_for_dump(archive)){
                break;
            }
            if(index >= 40){
                std::cout << "  [truncated]\n";
                break;
            }
        }
    }
}

void dump_object_body_sequences(
    BinaryArchive& archive,
    std::initializer_list<std::uint32_t> body_offsets,
    std::optional<std::uint32_t> class_id_filter = std::nullopt) {
    std::set<std::uint32_t> targets(body_offsets.begin(), body_offsets.end());

    while(true){
        const auto start_offset = static_cast<std::uint32_t>(archive.archive().state().input.offset);
        const auto header = archive.next();
        if(!header.hasItem){
            break;
        }

        if(header.signTypeCode != TypeCode::ID::BeginObject || !targets.contains(start_offset)){
            archive.doGet(TypeCode::ID::None);
            continue;
        }
        if(class_id_filter.has_value() && header.tempClassId != *class_id_filter){
            archive.doGet(TypeCode::ID::None);
            continue;
        }
        if(!archive.getObject()){
            break;
        }

        std::cout << std::format(
            "object body dump: offset=0x{:X} class_id=0x{:08X}\n",
            start_offset,
            header.tempClassId);
        std::size_t index = 0;
        while(true){
            const auto item = archive.next();
            if(!item.hasItem){
                break;
            }
            if(item.signTypeCode == TypeCode::ID::EndObject){
                if(!archive.get(TypeCode::ID::EndObject, 1)){
                    break;
                }
                std::cout << "  [end]\n";
                break;
            }

            std::cout << std::format(
                "  [{}] type={} meta={} class_id={} object_id={} payload_size={}\n",
                index,
                TypeCode::name(static_cast<std::uint8_t>(item.signTypeCode)),
                item.metaBytes.empty() ? "<none>" : reinterpret_cast<const char*>(item.metaBytes.data()),
                item.tempClassId,
                item.tempObjectId,
                item.payloadBytes.size());
            index += 1;
            if(!skip_item_for_dump(archive)){
                break;
            }
            if(index >= 40){
                std::cout << "  [truncated]\n";
                break;
            }
        }
    }
}

int inspect_archive_dictionary(std::string_view filepath) {
    BinaryFileReader reader{std::string(filepath)};
    if(!reader.read_all()){
        std::cerr << "read_all failed: " << reader.get_last_error() << "\n";
        return 1;
    }

    const auto& nodes = reader.get_nodes();
    if(nodes.empty()){
        std::cerr << "no nodes found\n";
        return 1;
    }

    std::size_t best_node = static_cast<std::size_t>(-1);
    std::size_t best_class_count = 0;
    std::size_t best_module_count = 0;

    for(std::size_t i = 0; i < nodes.size(); ++i){
        BinaryArchive archive;
        archive.reset(reader.read_data_block(i));
        archive.read_first_section();

        const auto& dictionaries = archive.archive().state().dictionaries;
        const auto class_count = dictionaries.classList.size();
        const auto module_count = dictionaries.moduleList.size();
        if(class_count == 0 && module_count == 0){
            continue;
        }

        best_node = i;
        best_class_count = class_count;
        best_module_count = module_count;
        if(class_count != 0 && module_count != 0){
            break;
        }
    }

    if(best_node == static_cast<std::size_t>(-1)){
        std::cerr << "no archive dictionary found in any node\n";
        return 1;
    }

    BinaryArchive archive;
    archive.reset(reader.read_data_block(best_node));
    archive.read_first_section();
    const auto& dictionaries = archive.archive().state().dictionaries;

    std::cout << std::format("dictionary node: {}\n", best_node);
    std::cout << std::format("class count: {}\n", dictionaries.classList.size());
    std::cout << std::format("module count: {}\n", dictionaries.moduleList.size());

    if(!dictionaries.classList.empty()){
        const auto& item = dictionaries.classList.front();
        std::cout << std::format(
            "first class: id={} name={} version={} module_id={}\n",
            item.class_id,
            item.name,
            item.version,
            item.module_id);
    }

    if(!dictionaries.moduleList.empty()){
        const auto& item = dictionaries.moduleList.front();
        std::cout << std::format(
            "first module: id={} name={} version={}\n",
            item.module_id,
            item.name,
            item.version);
    }

    dump_dictionary_lookup_samples(dictionaries, {
        1234567902u,
        58754297u,
    });

    if(dictionaries.classList.empty() || dictionaries.moduleList.empty()){
        std::cerr << "dictionary is incomplete: expected both ClassItem and ModuleItem\n";
        return 1;
    }

    archive.reset(reader.read_data_block(best_node));
    archive.read_first_section();
    archive.archive().state().parsing.itemHeader = {};
    std::cout << "dynamic type samples:\n";
    dump_dynamic_type_samples(archive, 0, 12);

    const auto countable_it = std::ranges::find_if(
        dictionaries.classList,
        [](const auto& item) { return item.name == "FTML::Countable"; });
    if(countable_it != dictionaries.classList.end()){
        archive.reset(reader.read_data_block(best_node));
        archive.read_first_section();
        archive.archive().state().parsing.itemHeader = {};
        std::cout << std::format(
            "countable compatibility samples: expected_class_id={}:\n",
            countable_it->class_id);
        dump_dynamic_type_samples(archive, countable_it->class_id, 8);
    }

    archive.reset(reader.read_data_block(best_node));
    archive.read_first_section();
    const auto snapshot = build_xmse_analysis_snapshot(archive, reader);
    dump_unified_testdat_report(snapshot);

    archive.reset(reader.read_data_block(best_node));
    archive.read_first_section();
    archive.archive().state().parsing.itemHeader = {};
    std::cout << "rawdata body sequences:\n";
    dump_rawdata_body_sequences(archive, {0x80Eu, 0x88Eu, 0xCCEu});

    archive.reset(reader.read_data_block(best_node));
    archive.read_first_section();
    archive.archive().state().parsing.itemHeader = {};
    std::cout << "object body sequences:\n";
    dump_object_body_sequences(archive, {0x7DDu}, 0x038084F9u);

    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if(argc == 1){
        return run_unit_tests();
    }

    return inspect_archive_dictionary(argv[1]);
}
