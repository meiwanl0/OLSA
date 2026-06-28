#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "FTMLBase.h"
#include "../container/ArchiveDictionary.h"
#include "../archive/SmartPointerFacade.h"

class BinaryArchive;

namespace FTML::XMSE {
struct DCRecipe {
    std::uint32_t numCyclesRef{};
    std::uint32_t numCyclesDark{};
    std::uint32_t numCyclesSample{};

    bool sumCycles{};

    bool wRangeHasMin{};
    bool wRangeHasMax{};
    float wRangeMin{};
    float wRangeMax{};

    float analyzerRef{};
    float analyzerSample{};
    float rotation{};

    bool symThreshHas{};
    float symThreshValue{};

    bool sumsPerCycleHas{};
    std::uint32_t sumsPerCycle{};

    bool timingModeHas{};
    std::uint32_t timingMode{};

    bool saturationHas{};
    std::uint32_t saturation{};
};

struct DPRecipe {
    std::int32_t binning{};
    std::string configApp;

    bool applyMultiScanErr{};
    bool applyPSF{};
    bool applyLinearity{};
    bool applyDCOffset{};
    bool applyA0P0Offset{};
    bool applyWShift{};
    bool applyTilt{};
    bool applyIDN{};

    bool modelTilt{};
};

struct RawData {
    struct UInt32ArrayPayload {
        std::uint32_t class_id{};
        std::uint64_t object_id{};
        bool is_2d{};
        std::uint32_t dim0{};
        std::uint32_t dim1{};
        std::vector<std::uint32_t> values;

        [[nodiscard]] std::size_t element_count() const {
            return values.size();
        }
    };

    std::uint32_t numSums{};
    std::uint32_t sumsPerCycle{};
    std::uint32_t timingMode{};
    std::int32_t numPixel{};
    std::uint32_t turnsPerCycle0{};
    std::uint32_t turnsPerCycle1{};
    std::uint32_t numBM{};
    std::uint32_t firstSum{};
    std::uint32_t firstAcqSum{};

    bool pixelRangeHasMin{};
    bool pixelRangeHasMax{};
    std::int32_t pixelRangeMin{};
    std::int32_t pixelRangeMax{};

    std::uint32_t clkPeriod{};
    std::uint32_t enc1Lines{};
    std::uint32_t enc2Lines{};

    std::optional<UInt32ArrayPayload> sig;
    std::optional<UInt32ArrayPayload> enc1;
    std::optional<UInt32ArrayPayload> enc2;
    std::optional<UInt32ArrayPayload> clk;
    std::optional<UInt32ArrayPayload> bm;
};

struct ConfigurationSetEntry {
    struct ConfigurationSummary {
        struct ConfigInfoEntrySummary {
            std::string key;
            std::uint32_t ordinal{};
            OLSA::Container::ResolvedTypeInfo type;
            OLSA::Container::ResolvedTypeInfo expected;
            FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
            std::uint32_t body_offset{};
            std::uint64_t object_id{};
            std::uint64_t tag{};
        };

        struct ConfigInfoSummary {
            std::vector<ConfigInfoEntrySummary> entries;
        };

        struct DCCalibrationEntrySummary {
            bool hasSettingsRaw{};
            std::uint32_t settingsRaw{};
            std::string settingsStdConfigName;
            OLSA::Container::ResolvedTypeInfo type;
            OLSA::Container::ResolvedTypeInfo expected;
            FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
            std::uint32_t body_offset{};
            std::uint64_t object_id{};
            std::uint64_t tag{};
        };

        struct DCTrailerSummary {
            bool hasExtracted{};
            std::uint32_t extracted{};
            std::string extractedStdConfigName;
            bool hasCalibrationsGroup{};
            std::vector<DCCalibrationEntrySummary> calibrationEntries;
        };

        std::string system;
        std::string comment;
        std::string baseName;
        bool hasTimestamp{};
        std::uint16_t timestampYear{};
        std::uint8_t timestampMonth{};
        std::uint8_t timestampDay{};
        std::uint8_t timestampHour{};
        std::uint8_t timestampMinute{};
        std::uint16_t timestampSubMinuteRaw{};
        bool hasConfigInfo{};
        OLSA::Container::ResolvedTypeInfo configInfoType;
        OLSA::Container::ResolvedTypeInfo configInfoExpected;
        FTML::SmartPointer::Compatibility configInfoCompatibility{FTML::SmartPointer::Compatibility::Unresolved};
        std::uint32_t configInfoBodyOffset{};
        std::uint64_t configInfoObjectId{};
        std::uint64_t configInfoTag{};
        std::optional<ConfigInfoSummary> configInfoSummary;
        std::optional<DCTrailerSummary> dcTrailerSummary;
        bool hasXMSETail{};
        std::uint32_t sumsPerCycle{};
        std::uint32_t timingMode{};
        std::uint32_t saturation{};
        std::uint32_t turnsPerCycle0{};
        std::uint32_t turnsPerCycle1{};
        std::uint32_t numPixel{};
        std::vector<std::string> observedFields;
    };

    struct NamedValueSetEntrySummary {
        std::string key;
        std::uint32_t ordinal{};
        OLSA::Container::ResolvedTypeInfo type;
        OLSA::Container::ResolvedTypeInfo expected;
        FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
        std::uint32_t body_offset{};
        std::uint64_t object_id{};
        std::uint64_t tag{};
        std::optional<ConfigurationSummary> configuration;
        struct NamedValueListSummary {
            struct StructuredEntry {
                std::string name;
                bool hasValueTag{};
                std::uint32_t valueTag{};
                OLSA::Container::ResolvedTypeInfo pointerType;
                OLSA::Container::ResolvedTypeInfo expected;
                FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
                std::uint32_t body_offset{};
                std::uint64_t object_id{};
                std::uint64_t tag{};
                std::optional<ConfigurationSummary> configuration;
            };

            std::size_t itemCount{};
            bool hasEntriesCount{};
            std::int32_t entriesCountRaw{};
            std::vector<std::string> preview;
            std::vector<StructuredEntry> structuredEntries;
        };
        std::optional<NamedValueListSummary> namedValueList;
    };

    struct NamedValueSetSummary {
        enum class ObservedValueEncoding {
            Unknown,
            TextPayload,
            NumericScalar,
        };

        enum class SemanticStatus {
            Unclassified,
            FullyRestored,
            ClosedWithCaveat,
            Opaque,
        };

        struct StructuredScalarEntry {
            std::string name;
            bool hasValueTag{};
            std::uint32_t valueTag{};
            ObservedValueEncoding encoding{ObservedValueEncoding::Unknown};
            std::string rawValueType;
            bool hasTextValue{};
            std::string textValue;
            bool hasSignedValue{};
            std::int32_t signedValue{};
            bool hasUnsignedValue{};
            std::uint32_t unsignedValue{};
            std::string valuePreview;
            // Relaxed node[3] closure metadata: this upgrades stable sample fields even when the
            // full consumer chain or enum table has not been fully recovered yet.
            SemanticStatus semanticStatus{SemanticStatus::Unclassified};
            std::string semanticMeaning;
            std::string semanticNote;
        };

        std::size_t itemCount{};
        std::vector<std::string> preview;
        std::vector<StructuredScalarEntry> structuredScalars;
        std::vector<NamedValueSetEntrySummary> entries;
    };

    std::string key;
    OLSA::Container::ResolvedTypeInfo type;
    OLSA::Container::ResolvedTypeInfo expected;
    FTML::SmartPointer::Compatibility compatibility{FTML::SmartPointer::Compatibility::Unresolved};
    std::uint32_t body_offset{};
    std::uint64_t object_id{};
    std::uint64_t tag{};
    std::optional<ConfigurationSummary> configuration;
    std::optional<NamedValueSetSummary> namedValueSet;
    std::optional<NamedValueSetEntrySummary::NamedValueListSummary> namedValueList;
};

struct ConfigurationSet {
    std::string systemID;
    bool usesPairLayout{};
    bool hasLegacyRoot{};
    ConfigurationSetEntry legacyRoot;
    std::vector<ConfigurationSetEntry> entries;
};

struct RawDataSet {
    struct TiltSummary {
        bool hasTheta{};
        float theta{};
        bool hasPhi{};
        float phi{};
        bool hasUnitsRaw{};
        std::uint32_t unitsRaw{};
        bool hasUnitsText{};
        std::string unitsText;
    };

    FTML::DC::RawDataSet dcRawDataSet{};
    bool hasSysInfo{};
    ConfigurationSetEntry sysInfo;
    std::int16_t version{};
    FTML::FlagValue<float> rotation{};
    FTML::FlagValue<float> analyzerRef{};
    FTML::FlagValue<float> analyzerSample{};
    bool hasTilt{};
    float tiltValue{};
    std::optional<TiltSummary> tiltSummary;
    FTML::FlagValue<float> pixShiftRef{};
    FTML::FlagValue<float> pixShiftSample{};
    bool hasDCRecipe{};
    bool hasDPRecipe{};
    void* dcRecipe{};
    void* dpRecipe{};
    FTML::NameString filterRef{};
    FTML::NameString filterSample{};
    std::string filterRefText;
    std::string filterSampleText;
    bool hasRef{};
    bool hasDark{};
    bool hasSample{};
    bool hasConfigSet{};
    void* ref{};
    void* dark{};
    void* sample{};
    void* configSet{};

    bool get(::BinaryArchive& ar);
};

enum class DynamicObjectKind {
    Unhandled,
    DCRecipe,
    DPRecipe,
    RawData,
    ConfigurationSet,
    RawDataSet
};

enum class DynamicReadStatus {
    InvalidStart,
    UnrecognizedType,
    UnsupportedType,
    NoMaterializedFields,
    Handled,
    ParseFailed
};

struct DynamicFieldTrace {
    std::string item_kind;
    std::string name;
    std::size_t depth{};
};

using DynamicObjectValue = std::variant<std::monostate, DCRecipe, DPRecipe, RawData, ConfigurationSet, RawDataSet>;

struct DynamicObjectReadResult {
    OLSA::Container::ResolvedTypeInfo type;
    DynamicObjectKind kind{DynamicObjectKind::Unhandled};
    DynamicObjectValue value{};
    DynamicReadStatus status{DynamicReadStatus::InvalidStart};
    bool started_from_pointer{};
    std::uint32_t body_offset{};
    std::uint64_t object_id{};
    std::uint64_t tag{};
    std::vector<DynamicFieldTrace> observed_fields;
    std::string detail;

    [[nodiscard]] bool recognized() const {
        return type.class_id != 0;
    }

    [[nodiscard]] bool handled() const {
        return kind != DynamicObjectKind::Unhandled;
    }

    template <class T>
    [[nodiscard]] const T* as() const {
        return std::get_if<T>(&value);
    }
};

bool read_dynamic(::BinaryArchive& ar, DynamicObjectReadResult& out);
[[nodiscard]] bool is_xmse_dynamic_type(std::string_view class_name);
[[nodiscard]] bool is_supported_dynamic_type(std::string_view class_name);
[[nodiscard]] std::vector<DynamicObjectReadResult> collect_recognized_dynamic_objects(::BinaryArchive& ar);
[[nodiscard]] std::vector<DynamicObjectReadResult> collect_supported_dynamic_objects(::BinaryArchive& ar);
}  // namespace FTML::XMSE
