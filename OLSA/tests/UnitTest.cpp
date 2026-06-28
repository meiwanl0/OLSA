#include "UnitTest.h"

#include "../include/BinaryArchive.h"
#include "../include/BinaryFileReader.h"
#include "../include/archive/Compress.h"
#include "../include/archive/Countable.h"
#include "../include/archive/TypeCompatibility.h"
#include "../include/protocol/ByteUtil.h"
#include "../include/SmartPointer.h"
#include "../include/XMSERawDataSet.h"

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>

namespace {
using TestFn = void(*)();

struct TestCase {
    std::string_view name;
    TestFn fn;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(std::string_view name, TestFn fn) {
        registry().push_back(TestCase{name, fn});
    }
};

struct TestState {
    int failed{};
};

TestState& state() {
    static TestState s;
    return s;
}

void check(bool ok, std::string_view expr, std::string_view file, int line) {
    if(ok){
        return;
    }
    state().failed += 1;
    std::cout << std::format("[FAIL] {}:{}  {}", file, line, expr) << "\n";
}
}  // namespace

#define TEST_CASE(name) \
    static void test_##name(); \
    static Registrar reg_##name{#name, &test_##name}; \
    static void test_##name()

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static_assert(ByteContainer<std::vector<std::uint8_t>>);

TEST_CASE(BinaryArchive_Get_UInt8_ConsumesItem) {
    BinaryArchive ar;
    ar.reset({static_cast<std::uint8_t>(TypeCode::ID::UInt8), 0x7F});
    auto& st = ar.archive().state();

    CHECK(ar.get(TypeCode::ID::UInt8, 1));
    CHECK(st.parsing.itemHeader.payloadBytes.size() == 1);
    CHECK(st.parsing.itemHeader.payloadBytes[0] == 0x7F);
    CHECK(!st.parsing.itemHeader.hasItem);
    CHECK(st.input.offset == 2);
}

TEST_CASE(ByteUtil_ReadLittleEndian_Float32) {
    const float f = 1.25f;
    const auto* p = reinterpret_cast<const std::uint8_t*>(&f);
    std::vector<std::uint8_t> bytes(p, p + sizeof(float));
    CHECK(read_little_endian<float>(as_bytes(bytes)) == f);
}

TEST_CASE(Compress_Unpack_RealUInt32ArrayBlockFromTestDat) {
    const std::vector<std::uint8_t> packed{
        0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xD9, 0xA6, 0x77, 0x86, 0x06, 0xC2, 0xF8, 0x05,
        0xA6, 0x77, 0x77, 0x86, 0x77, 0x77, 0x86, 0x20, 0x2E, 0xC2, 0x82, 0xC1, 0xC2, 0xC3, 0x0A, 0xC2,
        0x81, 0xC1, 0xC2, 0x20, 0x7C, 0xC2, 0x80, 0xC3, 0xF8, 0x02, 0x97, 0x77, 0x77, 0x68, 0x05, 0xC2,
        0x81, 0xC1, 0xC2, 0x20, 0x25, 0xC2, 0x81, 0xC1, 0xC2, 0x14, 0xC2, 0x81, 0xC1, 0xC2, 0x08, 0xC2,
        0x81, 0xC1, 0xC2, 0x05, 0xC2, 0x81, 0xC1, 0xC2, 0x13, 0xC2, 0x81, 0xC1, 0xC2, 0x15, 0xC2, 0x81,
        0xC1, 0xC2, 0x14, 0xC2, 0x81, 0xC1, 0xC2, 0x08, 0xC2, 0x81, 0xC1, 0xC1, 0x20, 0x20, 0xC2, 0x81,
        0xC1, 0xC2, 0x19, 0xC2, 0xF8, 0x02, 0x88, 0x77, 0x77, 0x68, 0x05, 0xC2, 0x81, 0xC1, 0xC2, 0x20,
        0x28, 0xC2, 0x20, 0x6C, 0x31, 0x81, 0x32, 0x31, 0x20, 0x4F, 0x31, 0x80, 0x32, 0xC7, 0x87, 0x77,
        0x68, 0x20, 0x48, 0x31, 0x80, 0x30, 0xE6, 0x04, 0x87, 0x77, 0x86, 0x77, 0x77, 0x86, 0x20, 0xB0,
        0x31, 0x80, 0x32, 0xE6, 0x02, 0x87, 0x77, 0x77, 0x86, 0x15, 0x31, 0xE6, 0x02, 0x96, 0x77, 0x77,
        0x86, 0x08, 0x31, 0x02, 0x32, 0x23, 0xFF, 0x08, 0x27, 0xFF, 0x00
    };

    Compress compress;
    std::vector<std::uint8_t> unpacked;
    CHECK(compress.unpack(std::span<const std::uint8_t>(packed.data(), packed.size()), unpacked));
    CHECK(unpacked.size() == 1024 * sizeof(std::uint32_t));
    CHECK(std::ranges::any_of(unpacked, [](std::uint8_t value) { return value != 0; }));
}

TEST_CASE(BinaryArchive_Next_ReadsCompressedFormattedUInt32Array) {
    const std::vector<std::uint8_t> packed{
        0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0xD9, 0xA6, 0x77, 0x86, 0x06, 0xC2, 0xF8, 0x05,
        0xA6, 0x77, 0x77, 0x86, 0x77, 0x77, 0x86, 0x20, 0x2E, 0xC2, 0x82, 0xC1, 0xC2, 0xC3, 0x0A, 0xC2,
        0x81, 0xC1, 0xC2, 0x20, 0x7C, 0xC2, 0x80, 0xC3, 0xF8, 0x02, 0x97, 0x77, 0x77, 0x68, 0x05, 0xC2,
        0x81, 0xC1, 0xC2, 0x20, 0x25, 0xC2, 0x81, 0xC1, 0xC2, 0x14, 0xC2, 0x81, 0xC1, 0xC2, 0x08, 0xC2,
        0x81, 0xC1, 0xC2, 0x05, 0xC2, 0x81, 0xC1, 0xC2, 0x13, 0xC2, 0x81, 0xC1, 0xC2, 0x15, 0xC2, 0x81,
        0xC1, 0xC2, 0x14, 0xC2, 0x81, 0xC1, 0xC2, 0x08, 0xC2, 0x81, 0xC1, 0xC1, 0x20, 0x20, 0xC2, 0x81,
        0xC1, 0xC2, 0x19, 0xC2, 0xF8, 0x02, 0x88, 0x77, 0x77, 0x68, 0x05, 0xC2, 0x81, 0xC1, 0xC2, 0x20,
        0x28, 0xC2, 0x20, 0x6C, 0x31, 0x81, 0x32, 0x31, 0x20, 0x4F, 0x31, 0x80, 0x32, 0xC7, 0x87, 0x77,
        0x68, 0x20, 0x48, 0x31, 0x80, 0x30, 0xE6, 0x04, 0x87, 0x77, 0x86, 0x77, 0x77, 0x86, 0x20, 0xB0,
        0x31, 0x80, 0x32, 0xE6, 0x02, 0x87, 0x77, 0x77, 0x86, 0x15, 0x31, 0xE6, 0x02, 0x96, 0x77, 0x77,
        0x86, 0x08, 0x31, 0x02, 0x32, 0x23, 0xFF, 0x08, 0x27, 0xFF, 0x00
    };

    BinaryArchive ar;
    std::vector<std::uint8_t> buffer;
    buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::UInt32) | 0x80);
    buffer.push_back(0x12);
    buffer.push_back(0x00);
    buffer.push_back(0x04);
    buffer.push_back(0xAB);
    buffer.insert(buffer.end(), packed.begin(), packed.end());
    ar.reset(std::move(buffer));
    auto& st = ar.archive().state();

    const auto header = ar.next();
    CHECK(header.hasItem);
    CHECK(header.signTypeCode == TypeCode::ID::UInt32);
    CHECK(header.payloadType == TypeCode::ID::UInt32);
    CHECK(header.payloadBytes.size() == 1024 * sizeof(std::uint32_t));
    CHECK(ar.get(TypeCode::ID::UInt32, 1024));
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(BinaryArchive_ReadFirstSection_SkipsBeginObject) {
    BinaryArchive ar;
    ar.reset({
        static_cast<std::uint8_t>(TypeCode::ID::BeginObject), 0x04, 0x50, 0x9F, 0xD6, 0x04,
        static_cast<std::uint8_t>(TypeCode::ID::Int32), 0x08, 0x00, 0x00, 0x00,
        static_cast<std::uint8_t>(TypeCode::ID::BeginGroup),
        static_cast<std::uint8_t>(TypeCode::ID::UInt32), 0x00, 0x00, 0x00, 0x00,
        static_cast<std::uint8_t>(TypeCode::ID::EndGroup),
        static_cast<std::uint8_t>(TypeCode::ID::BeginGroup),
        static_cast<std::uint8_t>(TypeCode::ID::UInt32), 0x00, 0x00, 0x00, 0x00,
        static_cast<std::uint8_t>(TypeCode::ID::EndGroup),
    });
    auto& st = ar.archive().state();

    ar.read_first_section();
    CHECK(read_little_endian<std::uint32_t>(as_bytes(st.parsing.itemHeader.payloadBytes)) == 0x00000000u);
    CHECK(st.input.offset >= 11);
}

TEST_CASE(BinaryArchive_GetPointer_ResolvesKnownType) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0x11223344u, "FTML::DemoType", 7, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x11223344u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLCore", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    ar.reset({
        static_cast<std::uint8_t>(TypeCode::ID::BeginPointer), 0x44,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55
    });

    CHECK(ar.getPointer());
    CHECK(st.parsing.resolvedType.has_value());
    CHECK(st.parsing.resolvedType->class_id == 0x11223344u);
    CHECK(st.parsing.resolvedType->class_name == "FTML::DemoType");
    CHECK(st.parsing.resolvedType->module_name == "FTMLCore");
    CHECK(st.parsing.resolvedType->module_version == "7.00.14");
    CHECK(st.parsing.resolvedType->is_pointer);
    CHECK(!st.parsing.resolvedType->from_tag);
}

TEST_CASE(SmartPointer_Extract_ReportsCompatibility) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0x01010101u, "FTML::Countable", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x01010101u, 0);
    st.dictionaries.classList.push_back({0x11223344u, "FTML::DemoType", 7, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x11223344u, 1);
    st.dictionaries.classList.push_back({0xAABBCCDDu, "FTML::OtherType", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xAABBCCDDu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLCore", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto reset_pointer = [&]() {
        ar.reset({
            static_cast<std::uint8_t>(TypeCode::ID::BeginPointer), 0x44,
            0x44, 0x33, 0x22, 0x11,
            0x88, 0x77, 0x66, 0x55
        });
    };

    FTML::SmartPointer::ExtractResult result{};

    reset_pointer();
    CHECK(FTML::SmartPointer::extract(ar, 0, result));
    CHECK(result.recognized());
    CHECK(result.compatibility == FTML::SmartPointer::Compatibility::NoExpectation);
    CHECK(result.matches_expected());
    CHECK(result.actual.class_name == "FTML::DemoType");

    reset_pointer();
    CHECK(FTML::SmartPointer::extract(ar, 0x11223344u, result));
    CHECK(result.compatibility == FTML::SmartPointer::Compatibility::Exact);
    CHECK(result.matches_expected());
    CHECK(result.expected.class_name == "FTML::DemoType");

    reset_pointer();
    CHECK(FTML::SmartPointer::extract(ar, 0x01010101u, result));
    CHECK(result.compatibility == FTML::SmartPointer::Compatibility::Derived);
    CHECK(result.matches_expected());
    CHECK(result.expected.class_name == "FTML::Countable");

    reset_pointer();
    CHECK(FTML::SmartPointer::extract(ar, 0xAABBCCDDu, result));
    CHECK(result.compatibility == FTML::SmartPointer::Compatibility::Different);
    CHECK(!result.matches_expected());
    CHECK(result.expected.class_name == "FTML::OtherType");
    CHECK(result.actual.class_name == "FTML::DemoType");
}

TEST_CASE(SmartPointer_Extract_UsesRecoveredExpectedTypeInfoWhenDictionaryMissesBaseType) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDE281900u, "FTML::XMSE::RawDataSet", 1, 0xDCCC9F00u});
    st.dictionaries.classIndexById.emplace(0xDE281900u, 0);
    st.dictionaries.moduleList.push_back({0xDCCC9F00u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0xDCCC9F00u, 0);

    ar.reset({
        static_cast<std::uint8_t>(TypeCode::ID::BeginPointer), 0x44,
        0x00, 0x19, 0x28, 0xDE,
        0x01, 0x00, 0x00, 0x00
    });

    FTML::SmartPointer::ExtractResult result{};
    CHECK(FTML::SmartPointer::extract(ar, 0x01A2C46Au, result));
    CHECK(result.recognized());
    CHECK(result.compatibility == FTML::SmartPointer::Compatibility::Derived);
    CHECK(result.matches_expected());
    CHECK(result.actual.class_name == "FTML::XMSE::RawDataSet");
    CHECK(result.expected.class_name == "FTML::DC::RawDataSet");
    CHECK(result.expected.module_name == "FTMLSysXMSE");
    CHECK(result.expected.known);
}

TEST_CASE(TypeCompatibility_UsesRecoveredClassIdLineage) {
    const OLSA::Container::ResolvedTypeInfo countable{.class_id = 0x001D4E16u};
    const OLSA::Container::ResolvedTypeInfo archive{.class_id = 0x001D3AC1u};
    const OLSA::Container::ResolvedTypeInfo binary_archive{.class_id = 0x04D69F50u};
    const OLSA::Container::ResolvedTypeInfo text_archive{.class_id = 0x002C7709u};
    const OLSA::Container::ResolvedTypeInfo array_base_type{.class_id = 0x001D3B7Fu};
    const OLSA::Container::ResolvedTypeInfo array_char_type{.class_id = 0x001D3FBEu};
    const OLSA::Container::ResolvedTypeInfo array_tagged_item_type{.class_id = 0x001D3B11u};
    const OLSA::Container::ResolvedTypeInfo printf_type{.class_id = 0x001D4C1Au};
    const OLSA::Container::ResolvedTypeInfo debug_type{.class_id = 0x001D445Eu};
    const OLSA::Container::ResolvedTypeInfo string_type{.class_id = 0x001D4F73u};
    const OLSA::Container::ResolvedTypeInfo wstring_type{.class_id = 0x001D4F94u};
    const OLSA::Container::ResolvedTypeInfo print_filter{.class_id = 0x127B0867u};
    const OLSA::Container::ResolvedTypeInfo scanner_type{.class_id = 0x001D60B9u};
    const OLSA::Container::ResolvedTypeInfo scan_queue_type{.class_id = 0x00C5031Bu};
    const OLSA::Container::ResolvedTypeInfo tagged_object_list_type{.class_id = 0x001D3AE9u};
    const OLSA::Container::ResolvedTypeInfo xmse_dc_recipe{.class_id = 0xDCF62C00u};
    const OLSA::Container::ResolvedTypeInfo dc_dc_recipe{.class_id = 0x0197F267u};
    const OLSA::Container::ResolvedTypeInfo xmse_dp_recipe{.class_id = 0xDCF65100u};
    const OLSA::Container::ResolvedTypeInfo dc_dp_recipe{.class_id = 0x0198277Bu};
    const OLSA::Container::ResolvedTypeInfo xmse_raw_data{.class_id = 0xDE284800u};
    const OLSA::Container::ResolvedTypeInfo xmse_raw_data_set{.class_id = 0xDE281900u};
    const OLSA::Container::ResolvedTypeInfo dc_raw_data_set{.class_id = 0x01A2C46Au};
    const OLSA::Container::ResolvedTypeInfo xmse_sub_system{.class_id = 0xDE36D000u};
    const OLSA::Container::ResolvedTypeInfo dc_sub_system{.class_id = 0x018514A1u};

    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(binary_archive, archive)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(binary_archive, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(text_archive, archive)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(archive, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(array_base_type, array_base_type)
        == FTML::SmartPointer::Compatibility::Exact);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(array_char_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(array_char_type, array_base_type)
        == FTML::SmartPointer::Compatibility::Different);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(array_tagged_item_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(array_tagged_item_type, array_char_type)
        == FTML::SmartPointer::Compatibility::Different);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(printf_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(debug_type, printf_type)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(debug_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(string_type, printf_type)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(string_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(wstring_type, printf_type)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(wstring_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(print_filter, printf_type)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(print_filter, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(scanner_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(scan_queue_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(scan_queue_type, scanner_type)
        == FTML::SmartPointer::Compatibility::Different);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(tagged_object_list_type, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(tagged_object_list_type, scanner_type)
        == FTML::SmartPointer::Compatibility::Different);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(xmse_dc_recipe, dc_dc_recipe)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(xmse_dp_recipe, dc_dp_recipe)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(xmse_raw_data, countable)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(xmse_raw_data_set, dc_raw_data_set)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(xmse_sub_system, dc_sub_system)
        == FTML::SmartPointer::Compatibility::Derived);
    CHECK(OLSA::ArchiveUtil::classify_type_compatibility(binary_archive, text_archive)
        == FTML::SmartPointer::Compatibility::Different);
}

TEST_CASE(XMSE_RawDataSet_Get_AssignsKeyFields) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };

    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };

    st.input.buffer.clear();

    const std::int16_t version = 1;
    push_meta_bytes(TypeCode::ID::Int16, "Version",
        {reinterpret_cast<const std::uint8_t*>(&version), sizeof(version)});

    const std::uint8_t stub = 0;
    push_meta_bytes(TypeCode::ID::UInt8, "DCRecipe", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "DPRecipe", {&stub, 1});

    const std::uint8_t rot_has = 1;
    push_meta_bytes(TypeCode::ID::Bool, "Rotation", {&rot_has, 1});
    const float rot_value = 1.25f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&rot_value), sizeof(rot_value)});

    const std::uint8_t aref_has = 0;
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerRef", {&aref_has, 1});

    const std::uint8_t asample_has = 1;
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerSample", {&asample_has, 1});
    const float asample_value = 2.5f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&asample_value), sizeof(asample_value)});

    const std::uint8_t tilt_has = 0;
    push_meta_bytes(TypeCode::ID::Bool, "Tilt", {&tilt_has, 1});

    const std::uint8_t psref_has = 0;
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftRef", {&psref_has, 1});

    const std::uint8_t pssample_has = 0;
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftSample", {&pssample_has, 1});

    push_meta_bytes(TypeCode::ID::UInt8, "FilterRef", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "FilterSample", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "Ref", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "Dark", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "Sample", {&stub, 1});
    push_meta_bytes(TypeCode::ID::UInt8, "ConfigSet", {&stub, 1});

    st.input.offset = 0;
    st.input.finalized = true;

    FTML::XMSE::RawDataSet ds{};
    CHECK(ds.get(ar));
    CHECK(ds.version == 1);
    CHECK(ds.rotation.has);
    CHECK(ds.rotation.value == rot_value);
    CHECK(!ds.analyzerRef.has);
    CHECK(ds.analyzerSample.has);
    CHECK(ds.analyzerSample.value == asample_value);
    CHECK(!ds.hasTilt);
    CHECK(!ds.pixShiftRef.has);
    CHECK(!ds.pixShiftSample.has);
    CHECK(!ds.hasDCRecipe);
    CHECK(!ds.hasDPRecipe);
    CHECK(ds.filterRefText.empty());
    CHECK(ds.filterSampleText.empty());
    CHECK(!ds.hasRef);
    CHECK(!ds.hasDark);
    CHECK(!ds.hasSample);
    CHECK(!ds.hasConfigSet);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_RawDataSetReadsTopLevelSummary) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDE281900u, "FTML::XMSE::RawDataSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDE281900u, 0);
    st.dictionaries.classList.push_back({0x499602DDu, "FTMLUtil::FTML::NamedValueTable", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x499602DDu, 1);
    st.dictionaries.classList.push_back({0x23000033u, "FTMLUtil::FTML::NamedValueSet", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000033u, 2);
    st.dictionaries.classList.push_back({0x23000044u, "FTMLUtil::FTML::NamedValueList", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000044u, 3);
    st.dictionaries.classList.push_back({0x30000002u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000002u, 4);
    st.dictionaries.classList.push_back({0x30000003u, "FTML::XMSE::DPRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000003u, 5);
    st.dictionaries.classList.push_back({0x30000004u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000004u, 6);
    st.dictionaries.classList.push_back({0x30000005u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000005u, 7);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLUtil", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_pointer_shell = [&](std::uint32_t class_id, std::uint32_t object_id, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::BeginPointer);
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::BeginPointer);
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array outer_header{
        std::uint8_t{0x00}, std::uint8_t{0x19}, std::uint8_t{0x28}, std::uint8_t{0xDE},
        std::uint8_t{0x70}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), outer_header.begin(), outer_header.end());
    push_string("sample-01");
    push_pointer_header(0x23000033u, 0x71u);
    push_string("Operator");
    push_pointer_header(0x23000044u, 0x81u);
    push_string("alice");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    const std::int16_t version = 1;
    push_meta_bytes(TypeCode::ID::Int16, "Version",
        {reinterpret_cast<const std::uint8_t*>(&version), sizeof(version)});
    push_pointer_shell(0x30000002u, 0x72u, "DCRecipe");
    push_pointer_shell(0x30000003u, 0x73u, "DPRecipe");

    const std::uint8_t has_false = 0;
    push_meta_bytes(TypeCode::ID::Bool, "Rotation", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerRef", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerSample", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "Tilt", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftRef", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftSample", {&has_false, 1});

    push_string("FilterRef-A", "FilterRef");
    push_string("FilterSample-B", "FilterSample");
    push_pointer_shell(0x30000004u, 0x74u, "Ref");
    push_pointer_shell(0x30000004u, 0x75u, "Dark");
    push_pointer_shell(0x30000004u, 0x76u, "Sample");
    push_pointer_shell(0x30000005u, 0x77u, "ConfigSet");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.kind == FTML::XMSE::DynamicObjectKind::RawDataSet);
    const auto* ds = result.as<FTML::XMSE::RawDataSet>();
    CHECK(ds != nullptr);
    if(ds == nullptr){
        return;
    }
    CHECK(ds->dcRawDataSet.sampleID == "sample-01");
    CHECK(ds->hasSysInfo);
    CHECK(ds->sysInfo.type.class_name == "FTMLUtil::FTML::NamedValueSet");
    CHECK(ds->sysInfo.expected.class_name == "FTMLUtil::FTML::NamedValueTable");
    CHECK(ds->sysInfo.compatibility == FTML::SmartPointer::Compatibility::Different);
    CHECK(ds->sysInfo.object_id == 0x71u);
    CHECK(ds->sysInfo.body_offset != 0);
    CHECK(ds->sysInfo.namedValueSet.has_value());
    if(!ds->sysInfo.namedValueSet.has_value()){
        return;
    }
    CHECK(ds->sysInfo.namedValueSet->entries.size() == 1);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars.empty());
    if(ds->sysInfo.namedValueSet->entries.size() != 1){
        return;
    }
    const auto& sysinfo_entry = ds->sysInfo.namedValueSet->entries[0];
    CHECK(sysinfo_entry.key == "Operator");
    CHECK(sysinfo_entry.type.class_name == "FTMLUtil::FTML::NamedValueList");
    CHECK(sysinfo_entry.object_id == 0x81u);
    CHECK(sysinfo_entry.namedValueList.has_value());
    if(!sysinfo_entry.namedValueList.has_value()){
        return;
    }
    CHECK(sysinfo_entry.namedValueList->itemCount == 1);
    CHECK(ds->version == version);
    CHECK(ds->filterRefText == "FilterRef-A");
    CHECK(ds->filterSampleText == "FilterSample-B");
    CHECK(ds->hasDCRecipe);
    CHECK(ds->hasDPRecipe);
    CHECK(ds->hasRef);
    CHECK(ds->hasDark);
    CHECK(ds->hasSample);
    CHECK(ds->hasConfigSet);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_RawDataSetReadsScalarSysInfoPreview) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDE281900u, "FTML::XMSE::RawDataSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDE281900u, 0);
    st.dictionaries.classList.push_back({0x499602DDu, "FTMLUtil::FTML::NamedValueTable", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x499602DDu, 1);
    st.dictionaries.classList.push_back({0x23000033u, "FTMLUtil::FTML::NamedValueSet", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000033u, 2);
    st.dictionaries.classList.push_back({0x30000002u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000002u, 3);
    st.dictionaries.classList.push_back({0x30000003u, "FTML::XMSE::DPRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000003u, 4);
    st.dictionaries.classList.push_back({0x30000004u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000004u, 5);
    st.dictionaries.classList.push_back({0x30000005u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x30000005u, 6);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLUtil", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_pointer_shell = [&](std::uint32_t class_id, std::uint32_t object_id, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::BeginPointer);
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::BeginPointer);
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array outer_header{
        std::uint8_t{0x00}, std::uint8_t{0x19}, std::uint8_t{0x28}, std::uint8_t{0xDE},
        std::uint8_t{0x70}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), outer_header.begin(), outer_header.end());
    push_string("sample-02");
    push_pointer_header(0x23000033u, 0x91u);
    const std::int8_t sysinfo_count = 1;
    push_scalar(TypeCode::ID::Int8, sysinfo_count);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    push_string("Operator");
    const std::uint8_t kind = 1;
    push_scalar(TypeCode::ID::UInt8, kind);
    push_string("alice");
    push_string("Slot");
    const std::uint8_t numeric_kind = 0x80;
    push_scalar(TypeCode::ID::UInt8, numeric_kind);
    const std::int8_t slot_value = 7;
    push_scalar(TypeCode::ID::Int8, slot_value);
    push_string("FieldX");
    push_scalar(TypeCode::ID::UInt8, numeric_kind);
    const std::int8_t field_x = -1;
    push_scalar(TypeCode::ID::Int8, field_x);
    push_string("AOI");
    const std::uint8_t text_kind = 0x4;
    push_scalar(TypeCode::ID::UInt8, text_kind);
    push_string("65");
    push_string("Acquisition Order");
    push_scalar(TypeCode::ID::UInt8, numeric_kind);
    const std::int32_t acquisition_order = -35718272;
    push_scalar(TypeCode::ID::Int32, acquisition_order);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    const std::int16_t version = 1;
    push_meta_bytes(TypeCode::ID::Int16, "Version",
        {reinterpret_cast<const std::uint8_t*>(&version), sizeof(version)});
    push_pointer_shell(0x30000002u, 0x72u, "DCRecipe");
    push_pointer_shell(0x30000003u, 0x73u, "DPRecipe");
    const std::uint8_t has_false = 0;
    push_meta_bytes(TypeCode::ID::Bool, "Rotation", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerRef", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "AnalyzerSample", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "Tilt", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftRef", {&has_false, 1});
    push_meta_bytes(TypeCode::ID::Bool, "PixShiftSample", {&has_false, 1});
    push_string("FilterRef-A", "FilterRef");
    push_string("FilterSample-B", "FilterSample");
    push_pointer_shell(0x30000004u, 0x74u, "Ref");
    push_pointer_shell(0x30000004u, 0x75u, "Dark");
    push_pointer_shell(0x30000004u, 0x76u, "Sample");
    push_pointer_shell(0x30000005u, 0x77u, "ConfigSet");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    const auto* ds = result.as<FTML::XMSE::RawDataSet>();
    CHECK(ds != nullptr);
    if(ds == nullptr){
        return;
    }
    CHECK(ds->hasSysInfo);
    CHECK(ds->sysInfo.namedValueSet.has_value());
    if(!ds->sysInfo.namedValueSet.has_value()){
        return;
    }
    CHECK(ds->sysInfo.namedValueSet->entries.empty());
    CHECK(ds->sysInfo.namedValueSet->structuredScalars.size() == 5);
    if(ds->sysInfo.namedValueSet->structuredScalars.size() != 5){
        return;
    }
    using SemanticStatus = FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::SemanticStatus;
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].name == "Operator");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].hasValueTag);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].valueTag == 1u);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].encoding
        == FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::Unknown);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].rawValueType == "text");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].hasTextValue);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].textValue == "alice");
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[0].hasSignedValue);
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[0].hasUnsignedValue);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].valuePreview == "text=alice");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[0].semanticStatus == SemanticStatus::Unclassified);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].name == "Slot");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].hasValueTag);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].valueTag == 0x80u);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].encoding
        == FTML::XMSE::ConfigurationSetEntry::NamedValueSetSummary::ObservedValueEncoding::NumericScalar);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].rawValueType == "int8");
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[1].hasTextValue);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].hasSignedValue);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].signedValue == 7);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].hasUnsignedValue);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].unsignedValue == 7u);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].valuePreview == "value=int8(7)");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[1].semanticStatus == SemanticStatus::Unclassified);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[2].name == "FieldX");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[2].semanticStatus == SemanticStatus::FullyRestored);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[2].semanticMeaning == "site/field X coordinate");
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[2].semanticNote.empty());
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[3].name == "AOI");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[3].semanticStatus == SemanticStatus::ClosedWithCaveat);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[3].semanticMeaning == "angle of incidence parameter");
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[3].semanticNote.empty());
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[4].name == "Acquisition Order");
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[4].semanticStatus == SemanticStatus::Opaque);
    CHECK(ds->sysInfo.namedValueSet->structuredScalars[4].semanticMeaning == "opaque acquisition-order metadata");
    CHECK(!ds->sysInfo.namedValueSet->structuredScalars[4].semanticNote.empty());
    CHECK(ds->sysInfo.namedValueSet->itemCount >= 4);
    CHECK(!ds->sysInfo.namedValueSet->preview.empty());
    CHECK(ds->sysInfo.namedValueSet->preview[0] == "value=int8(1)");
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_RawDataSetGet_ReadsAnonymousProvenLayout) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0x038084F9u, "FTML::Tilt", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x038084F9u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLBase", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_string = [&](std::string_view text) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_meta_string = [&](std::string_view name, std::string_view text) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::Char) | 0xC0);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_pointer_shell = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };
    auto push_tilt_object = [&](float theta, float phi, std::string_view units) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginObject));
        st.input.buffer.push_back(0x04);
        const std::array object_header{
            static_cast<std::uint8_t>(0x038084F9u & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 8) & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 16) & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), object_header.begin(), object_header.end());
        push_meta_bytes(TypeCode::ID::Float, "Theta", {reinterpret_cast<const std::uint8_t*>(&theta), sizeof(theta)});
        push_meta_bytes(TypeCode::ID::Float, "Phi", {reinterpret_cast<const std::uint8_t*>(&phi), sizeof(phi)});
        push_meta_string("Units", units);
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndObject));
    };

    st.input.buffer.clear();
    const std::int8_t sample_id_prefix = 0;
    push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&sample_id_prefix), sizeof(sample_id_prefix)});
    push_pointer_shell(0x499602DEu, 0x11111111u);

    const std::int8_t version = 2;
    push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&version), sizeof(version)});
    push_pointer_shell(0xDCDD0001u, 0x22222222u);
    push_pointer_shell(0xDCDD0002u, 0x33333333u);

    const std::uint8_t rotation_has = 1;
    const float rotation_value = 1.570796f;
    push_bytes(TypeCode::ID::Bool, {&rotation_has, 1});
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&rotation_value), sizeof(rotation_value)});

    const std::uint8_t analyzer_ref_has = 0;
    push_bytes(TypeCode::ID::Bool, {&analyzer_ref_has, 1});

    const std::uint8_t analyzer_sample_has = 1;
    const float analyzer_sample_value = -0.436332f;
    push_bytes(TypeCode::ID::Bool, {&analyzer_sample_has, 1});
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&analyzer_sample_value), sizeof(analyzer_sample_value)});

    const std::uint8_t tilt_has = 1;
    push_bytes(TypeCode::ID::Bool, {&tilt_has, 1});
    const float tilt_theta = 0.125f;
    const float tilt_phi = -0.5f;
    push_tilt_object(tilt_theta, tilt_phi, "deg");

    const std::uint8_t pix_shift_ref_has = 0;
    push_bytes(TypeCode::ID::Bool, {&pix_shift_ref_has, 1});

    const std::uint8_t pix_shift_sample_has = 1;
    const float pix_shift_sample_value = -0.001f;
    push_bytes(TypeCode::ID::Bool, {&pix_shift_sample_has, 1});
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&pix_shift_sample_value), sizeof(pix_shift_sample_value)});

    const std::uint8_t empty_filter = 0;
    push_bytes(TypeCode::ID::UInt8, {&empty_filter, 1});
    push_bytes(TypeCode::ID::UInt8, {&empty_filter, 1});
    push_pointer_shell(0xDCDD0003u, 0x44444444u);
    push_pointer_shell(0xDCDD0003u, 0x55555555u);
    push_pointer_shell(0xDCDD0003u, 0x66666666u);
    push_pointer_shell(0xDCDD0004u, 0x77777777u);

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::RawDataSet ds{};
    CHECK(ds.get(ar));
    CHECK(ds.version == 2);
    CHECK(ds.rotation.has);
    CHECK(ds.rotation.value == rotation_value);
    CHECK(!ds.analyzerRef.has);
    CHECK(ds.analyzerSample.has);
    CHECK(ds.analyzerSample.value == analyzer_sample_value);
    CHECK(ds.hasTilt);
    CHECK(ds.tiltSummary.has_value());
    if(ds.tiltSummary.has_value()){
        CHECK(ds.tiltSummary->hasTheta);
        CHECK(ds.tiltSummary->theta == tilt_theta);
        CHECK(ds.tiltSummary->hasPhi);
        CHECK(ds.tiltSummary->phi == tilt_phi);
        CHECK(ds.tiltSummary->hasUnitsText);
        CHECK(ds.tiltSummary->unitsText == "deg");
    }
    CHECK(!ds.pixShiftRef.has);
    CHECK(ds.pixShiftSample.has);
    CHECK(ds.pixShiftSample.value == pix_shift_sample_value);
    CHECK(ds.filterRefText.empty());
    CHECK(ds.filterSampleText.empty());
    CHECK(ds.hasDCRecipe);
    CHECK(ds.hasDPRecipe);
    CHECK(ds.hasRef);
    CHECK(ds.hasDark);
    CHECK(ds.hasSample);
    CHECK(ds.hasConfigSet);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_RawDataSetGet_MapsAnonymousTiltRawUnits) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0x038084F9u, "FTML::Tilt", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x038084F9u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLBase", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_pointer_shell = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array pointer_header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };
    auto push_tilt_object = [&](float theta, float phi, std::uint32_t units_raw) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginObject));
        st.input.buffer.push_back(0x04);
        const std::array object_header{
            static_cast<std::uint8_t>(0x038084F9u & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 8) & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 16) & 0xFFu),
            static_cast<std::uint8_t>((0x038084F9u >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), object_header.begin(), object_header.end());
        push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&theta), sizeof(theta)});
        push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&phi), sizeof(phi)});
        push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&units_raw), sizeof(units_raw)});
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndObject));
    };

    st.input.buffer.clear();
    const std::int8_t sample_id_prefix = 0;
    push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&sample_id_prefix), sizeof(sample_id_prefix)});
    push_pointer_shell(0x499602DEu, 0x11111111u);

    const std::int8_t version = 2;
    push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&version), sizeof(version)});
    push_pointer_shell(0xDCDD0001u, 0x22222222u);
    push_pointer_shell(0xDCDD0002u, 0x33333333u);

    const std::uint8_t rotation_has = 0;
    push_bytes(TypeCode::ID::Bool, {&rotation_has, 1});
    const float zero_float = 0.0f;

    const std::uint8_t analyzer_ref_has = 0;
    push_bytes(TypeCode::ID::Bool, {&analyzer_ref_has, 1});

    const std::uint8_t analyzer_sample_has = 0;
    push_bytes(TypeCode::ID::Bool, {&analyzer_sample_has, 1});

    const std::uint8_t tilt_has = 1;
    push_bytes(TypeCode::ID::Bool, {&tilt_has, 1});
    const float tilt_theta = 0.0f;
    const float tilt_phi = 0.0f;
    const std::uint32_t tilt_units_raw = 0x013148DAu;
    push_tilt_object(tilt_theta, tilt_phi, tilt_units_raw);

    const std::uint8_t pix_shift_ref_has = 0;
    push_bytes(TypeCode::ID::Bool, {&pix_shift_ref_has, 1});

    const std::uint8_t pix_shift_sample_has = 0;
    push_bytes(TypeCode::ID::Bool, {&pix_shift_sample_has, 1});

    const std::uint8_t empty_filter = 0;
    push_bytes(TypeCode::ID::UInt8, {&empty_filter, 1});
    push_bytes(TypeCode::ID::UInt8, {&empty_filter, 1});
    push_pointer_shell(0xDCDD0003u, 0x44444444u);
    push_pointer_shell(0xDCDD0003u, 0x55555555u);
    push_pointer_shell(0xDCDD0003u, 0x66666666u);
    push_pointer_shell(0xDCDD0004u, 0x77777777u);

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::RawDataSet ds{};
    CHECK(ds.get(ar));
    CHECK(ds.hasTilt);
    CHECK(ds.tiltSummary.has_value());
    if(ds.tiltSummary.has_value()){
        CHECK(ds.tiltSummary->hasUnitsRaw);
        CHECK(ds.tiltSummary->unitsRaw == tilt_units_raw);
        CHECK(ds.tiltSummary->hasUnitsText);
        CHECK(ds.tiltSummary->unitsText == "radians");
    }
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_DispatchesDCRecipe) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA001u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA001u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_meta_string = [&](std::string_view name, std::string_view text) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x40 | 0x80);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bool_field = [&](std::string_view name, bool value) {
        const std::uint8_t raw = value ? 1u : 0u;
        push_meta_bytes(TypeCode::ID::Bool, name, {&raw, 1});
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x01}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x78}, std::uint8_t{0x56}, std::uint8_t{0x34}, std::uint8_t{0x12}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    const std::uint32_t num_cycles_ref = 7;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesRef",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_ref), sizeof(num_cycles_ref)});
    const std::uint32_t num_cycles_dark = 3;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesDark",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_dark), sizeof(num_cycles_dark)});
    const std::uint32_t num_cycles_sample = 9;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesSample",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_sample), sizeof(num_cycles_sample)});
    push_bool_field("SumCycles", true);

    push_meta(TypeCode::ID::Bool, "WRange");
    const std::uint8_t wmin = 1;
    st.input.buffer.push_back(wmin);
    const std::uint8_t wmax = 1;
    push_bytes(TypeCode::ID::Bool, {&wmax, 1});
    const float wmin_value = 1.5f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&wmin_value), sizeof(wmin_value)});
    const float wmax_value = 3.5f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&wmax_value), sizeof(wmax_value)});

    const float analyzer_ref = 2.0f;
    push_meta_bytes(TypeCode::ID::Float, "AnalyzerRef",
        {reinterpret_cast<const std::uint8_t*>(&analyzer_ref), sizeof(analyzer_ref)});
    const float analyzer_sample = 2.5f;
    push_meta_bytes(TypeCode::ID::Float, "AnalyzerSample",
        {reinterpret_cast<const std::uint8_t*>(&analyzer_sample), sizeof(analyzer_sample)});
    const float rotation = 10.0f;
    push_meta_bytes(TypeCode::ID::Float, "Rotation",
        {reinterpret_cast<const std::uint8_t*>(&rotation), sizeof(rotation)});

    push_meta(TypeCode::ID::Bool, "SymThresh");
    const std::uint8_t sym_has = 1;
    st.input.buffer.push_back(sym_has);
    const float sym_value = 0.75f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&sym_value), sizeof(sym_value)});

    push_meta(TypeCode::ID::Bool, "SumsPerCycle");
    const std::uint8_t spc_has = 1;
    st.input.buffer.push_back(spc_has);
    const std::uint32_t spc = 12;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&spc), sizeof(spc)});

    push_meta(TypeCode::ID::Bool, "TimingMode");
    const std::uint8_t timing_has = 1;
    st.input.buffer.push_back(timing_has);
    const std::uint32_t timing_mode = 4;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});

    push_meta(TypeCode::ID::Bool, "Saturation");
    const std::uint8_t saturation_has = 1;
    st.input.buffer.push_back(saturation_has);
    const std::uint32_t saturation = 88;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&saturation), sizeof(saturation)});

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.started_from_pointer);
    CHECK(result.body_offset == 10);
    CHECK(result.object_id == 0x12345678u);
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.type.class_name == "FTML::XMSE::DCRecipe");
    CHECK(result.kind == FTML::XMSE::DynamicObjectKind::DCRecipe);
    CHECK(!result.observed_fields.empty());
    const auto* recipe = result.as<FTML::XMSE::DCRecipe>();
    CHECK(recipe != nullptr);
    CHECK(recipe->numCyclesRef == num_cycles_ref);
    CHECK(recipe->numCyclesDark == num_cycles_dark);
    CHECK(recipe->numCyclesSample == num_cycles_sample);
    CHECK(recipe->sumCycles);
    CHECK(recipe->wRangeHasMin);
    CHECK(recipe->wRangeHasMax);
    CHECK(recipe->wRangeMin == wmin_value);
    CHECK(recipe->wRangeMax == wmax_value);
    CHECK(recipe->timingModeHas);
    CHECK(recipe->timingMode == timing_mode);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_RawDataSkipsUnknownPointers) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA002u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA002u, 0);
    st.dictionaries.classList.push_back({0x11223344u, "FTML::UnknownSignal", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x11223344u, 1);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_unknown_pointer_field = [&](std::string_view name) {
        push_meta(TypeCode::ID::BeginPointer, name);
        st.input.buffer.push_back(0x44);
        const std::array pointer_bytes{
            std::uint8_t{0x44}, std::uint8_t{0x33}, std::uint8_t{0x22}, std::uint8_t{0x11},
            std::uint8_t{0x11}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
        };
        st.input.buffer.insert(st.input.buffer.end(), pointer_bytes.begin(), pointer_bytes.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x02}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x01}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    const std::uint32_t num_sums = 5;
    push_meta_bytes(TypeCode::ID::UInt32, "NumSums",
        {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 8;
    push_meta_bytes(TypeCode::ID::UInt32, "SumsPerCycle",
        {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 6;
    push_meta_bytes(TypeCode::ID::UInt32, "TimingMode",
        {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 1024;
    push_meta_bytes(TypeCode::ID::Int32, "NumPixel",
        {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});

    push_meta(TypeCode::ID::UInt32, "TurnsPerCycle");
    const std::uint32_t turn0 = 1;
    st.input.buffer.insert(st.input.buffer.end(),
        reinterpret_cast<const std::uint8_t*>(&turn0),
        reinterpret_cast<const std::uint8_t*>(&turn0) + sizeof(turn0));
    const std::uint32_t turn1 = 2;
    push_bytes(TypeCode::ID::UInt32,
        {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});

    push_unknown_pointer_field("Sig");

    const std::uint32_t clk_period = 99;
    push_meta_bytes(TypeCode::ID::UInt32, "ClkPeriod",
        {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 11;
    push_meta_bytes(TypeCode::ID::UInt32, "Enc1Lines",
        {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 12;
    push_meta_bytes(TypeCode::ID::UInt32, "Enc2Lines",
        {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.started_from_pointer);
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.kind == FTML::XMSE::DynamicObjectKind::RawData);
    CHECK(!result.observed_fields.empty());
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->numSums == num_sums);
    CHECK(raw->sumsPerCycle == sums_per_cycle);
    CHECK(raw->timingMode == timing_mode);
    CHECK(raw->numPixel == num_pixel);
    CHECK(raw->turnsPerCycle0 == turn0);
    CHECK(raw->turnsPerCycle1 == turn1);
    CHECK(raw->clkPeriod == clk_period);
    CHECK(raw->enc1Lines == enc1_lines);
    CHECK(raw->enc2Lines == enc2_lines);
    CHECK(!raw->sig.has_value());
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_RawDataReadsNamedKnownArrayPointers) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA010u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA010u, 0);
    st.dictionaries.classList.push_back({0x001D42B7u, "FTML::Array2D<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D42B7u, 1);
    st.dictionaries.classList.push_back({0x001D401Eu, "FTML::Array<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D401Eu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_pointer = [&](std::string_view name, std::uint32_t class_id, std::uint32_t object_id) {
        push_meta(TypeCode::ID::BeginPointer, name);
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };
    auto push_formatted_uint32_array = [&](std::span<const std::uint32_t> values) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::UInt32) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(values.size()));
        for(const auto value : values){
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
            st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
        }
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x10}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x42}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    const std::uint32_t num_sums = 41;
    push_meta_bytes(TypeCode::ID::UInt32, "NumSums", {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 42;
    push_meta_bytes(TypeCode::ID::UInt32, "SumsPerCycle", {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 43;
    push_meta_bytes(TypeCode::ID::UInt32, "TimingMode", {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 4;
    push_meta_bytes(TypeCode::ID::Int32, "NumPixel", {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});
    push_meta(TypeCode::ID::UInt32, "TurnsPerCycle");
    const std::uint32_t turn0 = 44;
    st.input.buffer.insert(st.input.buffer.end(), reinterpret_cast<const std::uint8_t*>(&turn0), reinterpret_cast<const std::uint8_t*>(&turn0) + sizeof(turn0));
    const std::uint32_t turn1 = 45;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});
    const std::uint32_t num_bm = 1;
    push_meta_bytes(TypeCode::ID::UInt32, "NumBM", {reinterpret_cast<const std::uint8_t*>(&num_bm), sizeof(num_bm)});
    const std::uint32_t first_sum = 46;
    push_meta_bytes(TypeCode::ID::UInt32, "FirstSum", {reinterpret_cast<const std::uint8_t*>(&first_sum), sizeof(first_sum)});
    const std::uint32_t first_acq_sum = 47;
    push_meta_bytes(TypeCode::ID::UInt32, "FirstAcqSum", {reinterpret_cast<const std::uint8_t*>(&first_acq_sum), sizeof(first_acq_sum)});

    const std::uint32_t clk_period = 4096;
    push_meta_bytes(TypeCode::ID::UInt32, "ClkPeriod", {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 500;
    push_meta_bytes(TypeCode::ID::UInt32, "Enc1Lines", {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 600;
    push_meta_bytes(TypeCode::ID::UInt32, "Enc2Lines", {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});

    push_pointer("Sig", 0x001D42B7u, 0x301u);
    const std::int8_t sig_rows = 2;
    push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&sig_rows), sizeof(sig_rows)});
    const std::int16_t sig_cols = 2;
    push_bytes(TypeCode::ID::Int16, {reinterpret_cast<const std::uint8_t*>(&sig_cols), sizeof(sig_cols)});
    const std::array<std::uint32_t, 4> sig_values{11u, 12u, 13u, 14u};
    push_formatted_uint32_array(sig_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer("Enc1", 0x001D401Eu, 0x302u);
    const std::array<std::uint32_t, 4> enc1_values{21u, 22u, 23u, 24u};
    push_formatted_uint32_array(enc1_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer("Enc2", 0x001D401Eu, 0x303u);
    const std::array<std::uint32_t, 4> enc2_values{31u, 32u, 33u, 34u};
    push_formatted_uint32_array(enc2_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer("Clk", 0x001D401Eu, 0x304u);
    const std::array<std::uint32_t, 4> clk_values{41u, 42u, 43u, 44u};
    push_formatted_uint32_array(clk_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer("BM", 0x001D401Eu, 0x305u);
    const std::array<std::uint32_t, 4> bm_values{51u, 52u, 53u, 54u};
    push_formatted_uint32_array(bm_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->sig.has_value());
    if(raw->sig.has_value()){
        CHECK(raw->sig->is_2d);
        CHECK(raw->sig->dim0 == 2u);
        CHECK(raw->sig->dim1 == 2u);
        CHECK(raw->sig->values == std::vector<std::uint32_t>({11u, 12u, 13u, 14u}));
    }
    CHECK(raw->enc1.has_value());
    if(raw->enc1.has_value()){
        CHECK(raw->enc1->values == std::vector<std::uint32_t>({21u, 22u, 23u, 24u}));
    }
    CHECK(raw->enc2.has_value());
    if(raw->enc2.has_value()){
        CHECK(raw->enc2->values == std::vector<std::uint32_t>({31u, 32u, 33u, 34u}));
    }
    CHECK(raw->clk.has_value());
    if(raw->clk.has_value()){
        CHECK(raw->clk->values == std::vector<std::uint32_t>({41u, 42u, 43u, 44u}));
    }
    CHECK(raw->bm.has_value());
    if(raw->bm.has_value()){
        CHECK(raw->bm->values == std::vector<std::uint32_t>({51u, 52u, 53u, 54u}));
    }
}

TEST_CASE(XMSE_ReadDynamic_AnonymousRawDataSkipsInlineArrayPointers) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA008u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA008u, 0);
    st.dictionaries.classList.push_back({0x001D42B7u, "FTML::Array2D<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D42B7u, 1);
    st.dictionaries.classList.push_back({0x001D401Eu, "FTML::Array<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D401Eu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bool = [&](bool value) {
        const std::uint8_t raw = value ? 1 : 0;
        push_bytes(TypeCode::ID::Bool, {&raw, 1});
    };
    auto push_pointer = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };
    auto push_formatted_uint32_array = [&](std::span<const std::uint32_t> values) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::UInt32) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(values.size()));
        for(const auto value : values){
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
            st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
        }
    };
    auto push_array2d_uint32_pointer = [&](std::uint32_t object_id, std::int8_t rows, std::int16_t cols) {
        push_pointer(0x001D42B7u, object_id);
        push_bytes(TypeCode::ID::Int8, {reinterpret_cast<const std::uint8_t*>(&rows), sizeof(rows)});
        push_bytes(TypeCode::ID::Int16, {reinterpret_cast<const std::uint8_t*>(&cols), sizeof(cols)});
        const std::array<std::uint32_t, 4> values{11u, 12u, 13u, 14u};
        push_formatted_uint32_array(values);
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };
    auto push_array_uint32_pointer = [&](std::uint32_t object_id, std::uint32_t base) {
        push_pointer(0x001D401Eu, object_id);
        const std::array<std::uint32_t, 4> values{base, base + 1, base + 2, base + 3};
        push_formatted_uint32_array(values);
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };

    st.input.buffer.clear();
    push_pointer(0xDCAAA008u, 0x40u);

    const std::uint32_t hw_setup = 7;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&hw_setup), sizeof(hw_setup)});
    const std::uint32_t num_sums = 21;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 22;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 23;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 4;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});
    const std::uint32_t turn0 = 24;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn0), sizeof(turn0)});
    const std::uint32_t turn1 = 25;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});
    const std::uint32_t num_bm = 1;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_bm), sizeof(num_bm)});
    const std::uint32_t first_sum = 26;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_sum), sizeof(first_sum)});
    const std::uint32_t first_acq_sum = 27;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_acq_sum), sizeof(first_acq_sum)});

    const std::uint16_t ts_year = 2026;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_year), sizeof(ts_year)});
    const std::uint8_t ts_month = 6;
    push_bytes(TypeCode::ID::UChar, {&ts_month, 1});
    const std::uint8_t ts_day = 27;
    push_bytes(TypeCode::ID::UChar, {&ts_day, 1});
    const std::uint8_t ts_hour = 11;
    push_bytes(TypeCode::ID::UChar, {&ts_hour, 1});
    const std::uint8_t ts_minute = 59;
    push_bytes(TypeCode::ID::UChar, {&ts_minute, 1});
    const std::uint16_t ts_millisecond = 512;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_millisecond), sizeof(ts_millisecond)});

    const std::int32_t pixel_range_flags = 0;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&pixel_range_flags), sizeof(pixel_range_flags)});

    const std::uint32_t clk_period = 1024;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 31250;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 62500;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});

    push_array2d_uint32_pointer(0x101u, 1, 4);
    push_array_uint32_pointer(0x102u, 100u);
    push_array_uint32_pointer(0x103u, 200u);
    push_array_uint32_pointer(0x104u, 300u);
    push_array_uint32_pointer(0x105u, 400u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->numSums == num_sums);
    CHECK(raw->sumsPerCycle == sums_per_cycle);
    CHECK(raw->timingMode == timing_mode);
    CHECK(raw->numPixel == num_pixel);
    CHECK(raw->turnsPerCycle0 == turn0);
    CHECK(raw->turnsPerCycle1 == turn1);
    CHECK(raw->numBM == num_bm);
    CHECK(raw->firstSum == first_sum);
    CHECK(raw->firstAcqSum == first_acq_sum);
    CHECK(!raw->pixelRangeHasMin);
    CHECK(!raw->pixelRangeHasMax);
    CHECK(raw->clkPeriod == clk_period);
    CHECK(raw->enc1Lines == enc1_lines);
    CHECK(raw->enc2Lines == enc2_lines);
    CHECK(raw->sig.has_value());
    if(raw->sig.has_value()){
        CHECK(raw->sig->is_2d);
        CHECK(raw->sig->dim0 == 1u);
        CHECK(raw->sig->dim1 == 4u);
        CHECK(raw->sig->values == std::vector<std::uint32_t>({11u, 12u, 13u, 14u}));
    }
    CHECK(raw->enc1.has_value());
    if(raw->enc1.has_value()){
        CHECK(raw->enc1->values == std::vector<std::uint32_t>({100u, 101u, 102u, 103u}));
    }
    CHECK(raw->enc2.has_value());
    if(raw->enc2.has_value()){
        CHECK(raw->enc2->values == std::vector<std::uint32_t>({200u, 201u, 202u, 203u}));
    }
    CHECK(raw->clk.has_value());
    if(raw->clk.has_value()){
        CHECK(raw->clk->values == std::vector<std::uint32_t>({300u, 301u, 302u, 303u}));
    }
    CHECK(raw->bm.has_value());
    if(raw->bm.has_value()){
        CHECK(raw->bm->values == std::vector<std::uint32_t>({400u, 401u, 402u, 403u}));
    }
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_AnonymousRawDataMaterializesInlineTaggedArrayPointers) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA009u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA009u, 0);
    st.dictionaries.classList.push_back({0x001D42B7u, "FTML::Array2D<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D42B7u, 1);
    st.dictionaries.classList.push_back({0x001D401Eu, "FTML::Array<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D401Eu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bool = [&](bool value) {
        const std::uint8_t raw = value ? 1 : 0;
        push_bytes(TypeCode::ID::Bool, {&raw, 1});
    };
    auto push_pointer = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };
    auto push_formatted_uint32_array = [&](std::span<const std::uint32_t> values) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::UInt32) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(values.size()));
        for(const auto value : values){
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
            st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
        }
    };
    auto push_uchar_string = [&](std::string_view text) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::UChar) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size() + 1));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
        st.input.buffer.push_back(0);
    };

    st.input.buffer.clear();
    push_pointer(0xDCAAA009u, 0x41u);

    push_uchar_string("Setup-A");
    const std::uint32_t num_sums = 5;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 32;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 33;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 4;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});
    const std::uint32_t turn0 = 34;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn0), sizeof(turn0)});
    const std::uint32_t turn1 = 35;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});
    const std::uint32_t num_bm = 0;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_bm), sizeof(num_bm)});
    const std::uint32_t first_sum = 36;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_sum), sizeof(first_sum)});
    const std::uint32_t first_acq_sum = 37;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_acq_sum), sizeof(first_acq_sum)});

    const std::uint16_t ts_year = 2026;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_year), sizeof(ts_year)});
    const std::uint8_t ts_month = 6;
    push_bytes(TypeCode::ID::UChar, {&ts_month, 1});
    const std::uint8_t ts_day = 27;
    push_bytes(TypeCode::ID::UChar, {&ts_day, 1});
    const std::uint8_t ts_hour = 12;
    push_bytes(TypeCode::ID::UChar, {&ts_hour, 1});
    const std::uint8_t ts_minute = 1;
    push_bytes(TypeCode::ID::UChar, {&ts_minute, 1});
    const std::uint16_t ts_millisecond = 128;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_millisecond), sizeof(ts_millisecond)});

    push_uchar_string("Max");
    const std::int32_t pixel_range_max = 4096;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&pixel_range_max), sizeof(pixel_range_max)});

    const std::uint32_t clk_period = 2048;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 1000;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 2000;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});

    push_pointer(0x001D42B7u, 0x201u);
    const std::uint8_t rows = 5;
    push_bytes(TypeCode::ID::UInt8, {&rows, 1});
    const std::int16_t cols = 4;
    push_bytes(TypeCode::ID::Int16, {reinterpret_cast<const std::uint8_t*>(&cols), sizeof(cols)});
    const std::array<std::uint32_t, 20> sig_values{
        1u, 2u, 3u, 4u, 5u,
        6u, 7u, 8u, 9u, 10u,
        11u, 12u, 13u, 14u, 15u,
        16u, 17u, 18u, 19u, 20u};
    push_formatted_uint32_array(sig_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer(0x001D401Eu, 0x202u);
    const std::array<std::uint32_t, 5> enc_values{10u, 11u, 12u, 13u, 14u};
    push_formatted_uint32_array(enc_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer(0x001D401Eu, 0x203u);
    const std::array<std::uint32_t, 5> enc2_values{20u, 21u, 22u, 23u, 24u};
    push_formatted_uint32_array(enc2_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_pointer(0x001D401Eu, 0x204u);
    const std::array<std::uint32_t, 5> clk_values{30u, 31u, 32u, 33u, 34u};
    push_formatted_uint32_array(clk_values);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.detail.find("check(false)=true") != std::string::npos);
    CHECK(result.detail.find("tail_eq_numSums=true") != std::string::npos);
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->numSums == num_sums);
    CHECK(!raw->pixelRangeHasMin);
    CHECK(raw->pixelRangeHasMax);
    CHECK(raw->pixelRangeMax == pixel_range_max);
    CHECK(raw->clkPeriod == clk_period);
    CHECK(raw->enc1Lines == enc1_lines);
    CHECK(raw->enc2Lines == enc2_lines);
    CHECK(raw->sig.has_value());
    if(raw->sig.has_value()){
        CHECK(raw->sig->is_2d);
        CHECK(raw->sig->dim0 == 5u);
        CHECK(raw->sig->dim1 == 4u);
        CHECK(raw->sig->values == std::vector<std::uint32_t>(
            {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u}));
    }
    CHECK(raw->enc1.has_value());
    if(raw->enc1.has_value()){
        CHECK(raw->enc1->values == std::vector<std::uint32_t>({10u, 11u, 12u, 13u, 14u}));
    }
    CHECK(raw->enc2.has_value());
    if(raw->enc2.has_value()){
        CHECK(raw->enc2->values == std::vector<std::uint32_t>({20u, 21u, 22u, 23u, 24u}));
    }
    CHECK(raw->clk.has_value());
    if(raw->clk.has_value()){
        CHECK(raw->clk->values == std::vector<std::uint32_t>({30u, 31u, 32u, 33u, 34u}));
    }
    CHECK(!raw->bm.has_value());
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_AnonymousRawDataKeepsTaggedShellPointersWithoutInlineBody) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA00Au, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA00Au, 0);
    st.dictionaries.classList.push_back({0x001D42B7u, "FTML::Array2D<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D42B7u, 1);
    st.dictionaries.classList.push_back({0x001D401Eu, "FTML::Array<unsigned>", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x001D401Eu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_pointer = [&](std::uint32_t class_id, std::uint64_t tag) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x84);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(tag & 0xFFu),
            static_cast<std::uint8_t>((tag >> 8) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 16) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 24) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 32) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 40) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 48) & 0xFFu),
            static_cast<std::uint8_t>((tag >> 56) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };
    auto push_object = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };
    st.input.buffer.clear();
    push_object(0xDCAAA00Au, 0x51u);

    const std::uint32_t hw_setup = 9;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&hw_setup), sizeof(hw_setup)});
    const std::uint32_t num_sums = 61;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 62;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 63;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 4;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});
    const std::uint32_t turn0 = 64;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn0), sizeof(turn0)});
    const std::uint32_t turn1 = 65;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});
    const std::uint32_t num_bm = 1;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_bm), sizeof(num_bm)});
    const std::uint32_t first_sum = 66;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_sum), sizeof(first_sum)});
    const std::uint32_t first_acq_sum = 67;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_acq_sum), sizeof(first_acq_sum)});

    const std::uint16_t ts_year = 2026;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_year), sizeof(ts_year)});
    const std::uint8_t ts_month = 6;
    push_bytes(TypeCode::ID::UChar, {&ts_month, 1});
    const std::uint8_t ts_day = 27;
    push_bytes(TypeCode::ID::UChar, {&ts_day, 1});
    const std::uint8_t ts_hour = 13;
    push_bytes(TypeCode::ID::UChar, {&ts_hour, 1});
    const std::uint8_t ts_minute = 7;
    push_bytes(TypeCode::ID::UChar, {&ts_minute, 1});
    const std::uint16_t ts_millisecond = 256;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_millisecond), sizeof(ts_millisecond)});

    const std::int32_t pixel_range_flags = 0;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&pixel_range_flags), sizeof(pixel_range_flags)});

    const std::uint32_t clk_period = 3072;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 777;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 888;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});

    push_pointer(0x001D42B7u, 0x301u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    push_pointer(0x001D401Eu, 0x302u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    push_pointer(0x001D401Eu, 0x303u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    push_pointer(0x001D401Eu, 0x304u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    push_pointer(0x001D401Eu, 0x305u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.detail.find("check(false)=false") != std::string::npos);
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->numSums == num_sums);
    CHECK(raw->sig.has_value());
    if(raw->sig.has_value()){
        CHECK(raw->sig->is_2d);
        CHECK(raw->sig->object_id == 0x301u);
        CHECK(raw->sig->values.empty());
    }
    CHECK(raw->enc1.has_value());
    if(raw->enc1.has_value()){
        CHECK(raw->enc1->object_id == 0x302u);
        CHECK(raw->enc1->values.empty());
    }
    CHECK(raw->enc2.has_value());
    if(raw->enc2.has_value()){
        CHECK(raw->enc2->object_id == 0x303u);
        CHECK(raw->enc2->values.empty());
    }
    CHECK(raw->clk.has_value());
    if(raw->clk.has_value()){
        CHECK(raw->clk->object_id == 0x304u);
        CHECK(raw->clk->values.empty());
    }
    CHECK(raw->bm.has_value());
    if(raw->bm.has_value()){
        CHECK(raw->bm->object_id == 0x305u);
        CHECK(raw->bm->values.empty());
    }
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_DispatchesBeginObject) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA003u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA003u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginObject));
    st.input.buffer.push_back(0x44);
    const std::array object_header{
        std::uint8_t{0x03}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0xEF}, std::uint8_t{0xBE}, std::uint8_t{0xAD}, std::uint8_t{0xDE}
    };
    st.input.buffer.insert(st.input.buffer.end(), object_header.begin(), object_header.end());

    const std::uint32_t num_cycles_ref = 15;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesRef",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_ref), sizeof(num_cycles_ref)});
    const std::uint32_t num_cycles_sample = 21;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesSample",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_sample), sizeof(num_cycles_sample)});
    const float rotation = 12.5f;
    push_meta_bytes(TypeCode::ID::Float, "Rotation",
        {reinterpret_cast<const std::uint8_t*>(&rotation), sizeof(rotation)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndObject));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(!result.started_from_pointer);
    CHECK(result.body_offset == 10);
    CHECK(result.object_id == 0xDEADBEEFu);
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.kind == FTML::XMSE::DynamicObjectKind::DCRecipe);
    CHECK(!result.type.is_pointer);
    const auto* recipe = result.as<FTML::XMSE::DCRecipe>();
    CHECK(recipe != nullptr);
    CHECK(recipe->numCyclesRef == num_cycles_ref);
    CHECK(recipe->numCyclesSample == num_cycles_sample);
    CHECK(recipe->rotation == rotation);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_CollectSupportedDynamicObjects_FiltersUnsupportedTypes) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA003u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA003u, 0);
    st.dictionaries.classList.push_back({0x11223344u, "FTML::XMSE::SubSystem", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x11223344u, 1);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginObject));
    st.input.buffer.push_back(0x44);
    const std::array object_header{
        std::uint8_t{0x03}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0xEF}, std::uint8_t{0xBE}, std::uint8_t{0xAD}, std::uint8_t{0xDE}
    };
    st.input.buffer.insert(st.input.buffer.end(), object_header.begin(), object_header.end());

    const std::uint32_t num_cycles_ref = 15;
    push_meta_bytes(TypeCode::ID::UInt32, "NumCyclesRef",
        {reinterpret_cast<const std::uint8_t*>(&num_cycles_ref), sizeof(num_cycles_ref)});
    const float rotation = 12.5f;
    push_meta_bytes(TypeCode::ID::Float, "Rotation",
        {reinterpret_cast<const std::uint8_t*>(&rotation), sizeof(rotation)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndObject));

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array unsupported_pointer{
        std::uint8_t{0x44}, std::uint8_t{0x33}, std::uint8_t{0x22}, std::uint8_t{0x11},
        std::uint8_t{0x02}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), unsupported_pointer.begin(), unsupported_pointer.end());
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    const auto recognized = FTML::XMSE::collect_recognized_dynamic_objects(ar);
    CHECK(recognized.size() == 2);
    CHECK(recognized.front().type.class_name == "FTML::XMSE::DCRecipe");
    CHECK(recognized.back().type.class_name == "FTML::XMSE::SubSystem");
    CHECK(recognized.back().status == FTML::XMSE::DynamicReadStatus::UnsupportedType);
    CHECK(st.input.offset == st.input.buffer.size());

    st.input.offset = 0;
    st.parsing.itemHeader = {};
    const auto results = FTML::XMSE::collect_supported_dynamic_objects(ar);
    CHECK(results.size() == 1);
    CHECK(results.front().recognized());
    CHECK(results.front().handled());
    CHECK(results.front().kind == FTML::XMSE::DynamicObjectKind::DCRecipe);
    CHECK(results.front().type.class_name == "FTML::XMSE::DCRecipe");
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_TracesNoMaterializedFields) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA004u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA004u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_meta = [&](TypeCode::ID id, std::string_view name) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id) | 0x40);
        st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
        st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
    };
    auto push_meta_bytes = [&](TypeCode::ID id, std::string_view name, std::span<const std::uint8_t> bytes) {
        push_meta(id, name);
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x04}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x02}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    const std::uint32_t unknown_value = 77;
    push_meta_bytes(TypeCode::ID::UInt32, "UnknownField",
        {reinterpret_cast<const std::uint8_t*>(&unknown_value), sizeof(unknown_value)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(!result.handled());
    CHECK(result.started_from_pointer);
    CHECK(result.body_offset == 10);
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::NoMaterializedFields);
    CHECK(result.observed_fields.size() == 1);
    CHECK(result.observed_fields.front().name == "UnknownField");
    CHECK(result.observed_fields.front().depth == 0);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_AnonymousDCRecipePayload) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA005u, "FTML::XMSE::DCRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA005u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bool = [&](bool value) {
        const std::uint8_t raw = value ? 1 : 0;
        push_bytes(TypeCode::ID::Bool, {&raw, 1});
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x05}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x44}, std::uint8_t{0x33}, std::uint8_t{0x22}, std::uint8_t{0x11}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    const std::uint32_t num_cycles_ref = 3;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_cycles_ref), sizeof(num_cycles_ref)});
    const std::uint32_t num_cycles_dark = 4;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_cycles_dark), sizeof(num_cycles_dark)});
    const std::uint32_t num_cycles_sample = 5;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_cycles_sample), sizeof(num_cycles_sample)});
    push_bool(true);
    push_bool(true);
    push_bool(false);
    const float wmin = 1.25f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&wmin), sizeof(wmin)});
    const float analyzer_ref = 2.25f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&analyzer_ref), sizeof(analyzer_ref)});
    const float analyzer_sample = 3.25f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&analyzer_sample), sizeof(analyzer_sample)});
    const float rotation = 4.5f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&rotation), sizeof(rotation)});
    push_bool(true);
    const float sym = 0.5f;
    push_bytes(TypeCode::ID::Float, {reinterpret_cast<const std::uint8_t*>(&sym), sizeof(sym)});
    push_bool(true);
    const std::uint32_t sums_per_cycle = 17;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    push_bool(false);
    push_bool(true);
    const std::uint32_t saturation = 91;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&saturation), sizeof(saturation)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* recipe = result.as<FTML::XMSE::DCRecipe>();
    CHECK(recipe != nullptr);
    CHECK(recipe->numCyclesRef == num_cycles_ref);
    CHECK(recipe->numCyclesDark == num_cycles_dark);
    CHECK(recipe->numCyclesSample == num_cycles_sample);
    CHECK(recipe->wRangeHasMin);
    CHECK(!recipe->wRangeHasMax);
    CHECK(recipe->wRangeMin == wmin);
    CHECK(recipe->rotation == rotation);
    CHECK(recipe->symThreshHas);
    CHECK(recipe->symThreshValue == sym);
    CHECK(!recipe->timingModeHas);
    CHECK(recipe->saturationHas);
    CHECK(recipe->saturation == saturation);
}

TEST_CASE(XMSE_ReadDynamic_AnonymousRawDataScansPastPrefix) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA006u, "FTML::XMSE::RawData", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA006u, 0);
    st.dictionaries.classList.push_back({0x11223344u, "FTML::UnknownSignal", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x11223344u, 1);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_bytes = [&](TypeCode::ID id, std::span<const std::uint8_t> bytes) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        st.input.buffer.insert(st.input.buffer.end(), bytes.begin(), bytes.end());
    };
    auto push_bool = [&](bool value) {
        const std::uint8_t raw = value ? 1 : 0;
        push_bytes(TypeCode::ID::Bool, {&raw, 1});
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array pointer_header{
        std::uint8_t{0x06}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x10}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), pointer_header.begin(), pointer_header.end());

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array unknown_pointer{
        std::uint8_t{0x44}, std::uint8_t{0x33}, std::uint8_t{0x22}, std::uint8_t{0x11},
        std::uint8_t{0x01}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), unknown_pointer.begin(), unknown_pointer.end());
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    const std::uint32_t hw_setup = 0;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&hw_setup), sizeof(hw_setup)});
    const std::uint32_t num_sums = 7;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_sums), sizeof(num_sums)});
    const std::uint32_t sums_per_cycle = 8;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&sums_per_cycle), sizeof(sums_per_cycle)});
    const std::uint32_t timing_mode = 9;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&timing_mode), sizeof(timing_mode)});
    const std::int32_t num_pixel = 1024;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&num_pixel), sizeof(num_pixel)});
    const std::uint32_t turn0 = 2;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn0), sizeof(turn0)});
    const std::uint32_t turn1 = 3;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&turn1), sizeof(turn1)});
    const std::uint32_t num_bm = 1;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&num_bm), sizeof(num_bm)});
    const std::uint32_t first_sum = 11;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_sum), sizeof(first_sum)});
    const std::uint32_t first_acq_sum = 12;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&first_acq_sum), sizeof(first_acq_sum)});
    const std::uint16_t ts_year = 2026;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_year), sizeof(ts_year)});
    const std::uint8_t ts_month = 6;
    push_bytes(TypeCode::ID::UChar, {&ts_month, 1});
    const std::uint8_t ts_day = 27;
    push_bytes(TypeCode::ID::UChar, {&ts_day, 1});
    const std::uint8_t ts_hour = 10;
    push_bytes(TypeCode::ID::UChar, {&ts_hour, 1});
    const std::uint8_t ts_minute = 33;
    push_bytes(TypeCode::ID::UChar, {&ts_minute, 1});
    const std::uint16_t ts_millisecond = 512;
    push_bytes(TypeCode::ID::UInt16, {reinterpret_cast<const std::uint8_t*>(&ts_millisecond), sizeof(ts_millisecond)});
    const std::int32_t pixel_range_flags = 3;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&pixel_range_flags), sizeof(pixel_range_flags)});
    const std::int32_t range_min = 33;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&range_min), sizeof(range_min)});
    const std::int32_t range_max = 66;
    push_bytes(TypeCode::ID::Int32, {reinterpret_cast<const std::uint8_t*>(&range_max), sizeof(range_max)});
    const std::uint32_t clk_period = 88;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&clk_period), sizeof(clk_period)});
    const std::uint32_t enc1_lines = 99;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc1_lines), sizeof(enc1_lines)});
    const std::uint32_t enc2_lines = 111;
    push_bytes(TypeCode::ID::UInt32, {reinterpret_cast<const std::uint8_t*>(&enc2_lines), sizeof(enc2_lines)});
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(!result.observed_fields.empty());
    CHECK(result.observed_fields.front().item_kind == "Skip");
    const auto* raw = result.as<FTML::XMSE::RawData>();
    CHECK(raw != nullptr);
    CHECK(raw->numSums == num_sums);
    CHECK(raw->numPixel == num_pixel);
    CHECK(raw->turnsPerCycle0 == turn0);
    CHECK(raw->turnsPerCycle1 == turn1);
    CHECK(raw->pixelRangeHasMin);
    CHECK(raw->pixelRangeHasMax);
    CHECK(raw->pixelRangeMin == range_min);
    CHECK(raw->pixelRangeMax == range_max);
    CHECK(raw->clkPeriod == clk_period);
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsSystemIdAndEntries) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA050u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA050u, 0);
    st.dictionaries.classList.push_back({0x21000001u, "FTML::DC::Configuration", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x21000001u, 1);
    st.dictionaries.classList.push_back({0x21000002u, "FTML::DC::Configuration", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x21000002u, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_pointer = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    };

    st.input.buffer.clear();
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
    st.input.buffer.push_back(0x44);
    const std::array outer_header{
        std::uint8_t{0x50}, std::uint8_t{0xA0}, std::uint8_t{0xAA}, std::uint8_t{0xDC},
        std::uint8_t{0x70}, std::uint8_t{0x00}, std::uint8_t{0x00}, std::uint8_t{0x00}
    };
    st.input.buffer.insert(st.input.buffer.end(), outer_header.begin(), outer_header.end());
    push_string("XMSE-System", "SystemID");
    push_string("Dark");
    push_pointer(0x21000001u, 0x81u);
    push_string("Sample");
    push_pointer(0x21000002u, 0x82u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.recognized());
    CHECK(result.handled());
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    CHECK(result.kind == FTML::XMSE::DynamicObjectKind::ConfigurationSet);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->systemID == "XMSE-System");
    CHECK(config_set->usesPairLayout);
    CHECK(!config_set->hasLegacyRoot);
    CHECK(config_set->entries.size() == 2);
    CHECK(config_set->entries[0].key == "Dark");
    CHECK(config_set->entries[0].type.class_name == "FTML::DC::Configuration");
    CHECK(config_set->entries[0].expected.class_id == 0x491A1E3Cu);
    CHECK(config_set->entries[1].key == "Sample");
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsAnonymousConfigurationSummary) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA051u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA051u, 0);
    st.dictionaries.classList.push_back({0x21000011u, "FTML::DC::Configuration", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x21000011u, 1);
    st.dictionaries.classList.push_back({0x22000022u, "FTML::DC::ConfigInfo", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x22000022u, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA051u, 0x90u);
    push_string("XMSE-System");
    push_string("Dark");
    push_pointer_header(0x21000011u, 0x91u);
    push_string("SYS");
    push_string("comment-text");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 27;
    const std::uint8_t hour = 17;
    const std::uint8_t minute = 25;
    const std::uint16_t millisecond = 456;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, millisecond);
    push_string("base-name");
    push_pointer_header(0x22000022u, 0x92u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->systemID == "XMSE-System");
    CHECK(config_set->entries.size() == 1);
    if(config_set->entries.size() != 1){
        return;
    }
    CHECK(config_set->entries[0].type.class_name == "FTML::DC::Configuration");
    CHECK(config_set->entries[0].configuration.has_value());
    if(!config_set->entries[0].configuration.has_value()){
        return;
    }
    CHECK(config_set->entries[0].configuration->system == "SYS");
    CHECK(config_set->entries[0].configuration->comment == "comment-text");
    CHECK(config_set->entries[0].configuration->hasTimestamp);
    CHECK(config_set->entries[0].configuration->timestampYear == 2026);
    CHECK(config_set->entries[0].configuration->timestampMonth == 6);
    CHECK(config_set->entries[0].configuration->timestampDay == 27);
    CHECK(config_set->entries[0].configuration->timestampHour == 17);
    CHECK(config_set->entries[0].configuration->timestampMinute == 25);
    CHECK(config_set->entries[0].configuration->timestampSubMinuteRaw == 456);
    CHECK(config_set->entries[0].configuration->baseName == "base-name");
    CHECK(config_set->entries[0].configuration->hasConfigInfo);
    CHECK(config_set->entries[0].configuration->configInfoType.class_name == "FTML::DC::ConfigInfo");
    CHECK(config_set->entries[0].configuration->configInfoObjectId == 0x92u);
    CHECK(config_set->entries[0].configuration->configInfoBodyOffset != 0);
    CHECK(!config_set->entries[0].configuration->hasXMSETail);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsAnonymousCompactXMSETailAfterDCTrailer) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA091u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA091u, 0);
    st.dictionaries.classList.push_back({0x23000011u, "FTML::XMSE::Configuration", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x23000011u, 1);
    st.dictionaries.classList.push_back({0x22000022u, "FTML::DC::ConfigInfo", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x22000022u, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA091u, 0xB0u);
    push_string("XMSE-System");
    push_string("TF");
    push_pointer_header(0x23000011u, 0xB1u);
    push_string("SYS");
    push_string("generated-config");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 28;
    const std::uint8_t hour = 9;
    const std::uint8_t minute = 14;
    const std::uint16_t sub_minute = 950;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, sub_minute);
    push_string("@Default");
    push_pointer_header(0x22000022u, 0xB2u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    const std::uint16_t extracted = 0x0142u;
    push_scalar(TypeCode::ID::UInt16, extracted);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));

    const std::uint16_t sums_per_cycle = 0x0140u;
    const std::uint8_t timing_mode = 3u;
    const std::uint16_t saturation = 0xFA00u;
    const std::uint8_t turns_per_cycle0 = 5u;
    const std::uint8_t turns_per_cycle1 = 1u;
    const std::int16_t num_pixel = 0x0400;
    push_scalar(TypeCode::ID::UInt16, sums_per_cycle);
    push_scalar(TypeCode::ID::UInt8, timing_mode);
    push_scalar(TypeCode::ID::UInt16, saturation);
    push_scalar(TypeCode::ID::UInt8, turns_per_cycle0);
    push_scalar(TypeCode::ID::UInt8, turns_per_cycle1);
    push_scalar(TypeCode::ID::Int16, num_pixel);

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->entries.size() == 1);
    if(config_set->entries.size() != 1){
        return;
    }
    CHECK(config_set->entries[0].type.class_name == "FTML::XMSE::Configuration");
    CHECK(config_set->entries[0].configuration.has_value());
    if(!config_set->entries[0].configuration.has_value()){
        return;
    }
    CHECK(config_set->entries[0].configuration->hasConfigInfo);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary.has_value());
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->hasExtracted);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->extracted == 0x142u);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->extractedStdConfigName == "XMSErprc65");
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->hasCalibrationsGroup);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries.empty());
    CHECK(config_set->entries[0].configuration->hasXMSETail);
    CHECK(config_set->entries[0].configuration->sumsPerCycle == 0x140u);
    CHECK(config_set->entries[0].configuration->timingMode == 3u);
    CHECK(config_set->entries[0].configuration->saturation == 0xFA00u);
    CHECK(config_set->entries[0].configuration->turnsPerCycle0 == 5u);
    CHECK(config_set->entries[0].configuration->turnsPerCycle1 == 1u);
    CHECK(config_set->entries[0].configuration->numPixel == 0x400u);
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetSkipsNonEmptyDCCalibrationGroupBeforeCompactXMSETail) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA091u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA091u, 0);
    st.dictionaries.classList.push_back({0x23000011u, "FTML::XMSE::Configuration", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x23000011u, 1);
    st.dictionaries.classList.push_back({0x22000022u, "FTML::DC::ConfigInfo", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x22000022u, 2);
    st.dictionaries.classList.push_back({0x49087FA9u, "FTML::Calibration", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x49087FA9u, 3);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLBase", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA091u, 0xC0u);
    push_string("XMSE-System");
    push_string("legacyRoot");
    push_pointer_header(0x23000011u, 0xC1u);
    push_string("SYS");
    push_string("legacy-config");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 28;
    const std::uint8_t hour = 9;
    const std::uint8_t minute = 30;
    const std::uint16_t sub_minute = 123;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, sub_minute);
    push_string("@Default");
    push_pointer_header(0x22000022u, 0xC2u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    const std::uint16_t extracted = 0x0142u;
    push_scalar(TypeCode::ID::UInt16, extracted);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));

    // Model the real legacyRoot pattern: compact Settings value followed by a Calibration pointer body.
    push_scalar(TypeCode::ID::UInt16, extracted);
    push_pointer_header(0x49087FA9u, 0xC3u);
    push_string("eTD: set FTML::AOICal, ");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    push_scalar(TypeCode::ID::UInt16, extracted);
    push_pointer_header(0x49087FA9u, 0xC4u);
    push_string("eTD: set FTML::NACal, ");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));

    const std::uint8_t sums_per_cycle = 0xA0u;
    const std::uint8_t timing_mode = 3u;
    const std::uint16_t saturation = 0xFA00u;
    const std::uint8_t turns_per_cycle0 = 5u;
    const std::uint8_t turns_per_cycle1 = 1u;
    const std::int16_t num_pixel = 0x0400;
    push_scalar(TypeCode::ID::UInt8, sums_per_cycle);
    push_scalar(TypeCode::ID::UInt8, timing_mode);
    push_scalar(TypeCode::ID::UInt16, saturation);
    push_scalar(TypeCode::ID::UInt8, turns_per_cycle0);
    push_scalar(TypeCode::ID::UInt8, turns_per_cycle1);
    push_scalar(TypeCode::ID::Int16, num_pixel);

    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->entries.size() == 1);
    if(config_set->entries.size() != 1){
        return;
    }
    CHECK(config_set->entries[0].configuration.has_value());
    if(!config_set->entries[0].configuration.has_value()){
        return;
    }
    CHECK(config_set->entries[0].configuration->hasConfigInfo);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary.has_value());
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->hasExtracted);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->extracted == 0x142u);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->extractedStdConfigName == "XMSErprc65");
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->hasCalibrationsGroup);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries.size() == 2);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[0].hasSettingsRaw);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[0].settingsRaw == 0x142u);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[0].settingsStdConfigName == "XMSErprc65");
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[0].object_id == 0xC3u);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[1].hasSettingsRaw);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[1].settingsRaw == 0x142u);
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[1].settingsStdConfigName == "XMSErprc65");
    CHECK(config_set->entries[0].configuration->dcTrailerSummary->calibrationEntries[1].object_id == 0xC4u);
    CHECK(config_set->entries[0].configuration->hasXMSETail);
    CHECK(config_set->entries[0].configuration->sumsPerCycle == 0xA0u);
    CHECK(config_set->entries[0].configuration->timingMode == 3u);
    CHECK(config_set->entries[0].configuration->saturation == 0xFA00u);
    CHECK(config_set->entries[0].configuration->turnsPerCycle0 == 5u);
    CHECK(config_set->entries[0].configuration->turnsPerCycle1 == 1u);
    CHECK(config_set->entries[0].configuration->numPixel == 0x400u);
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsConfigInfoNamedValueTableSummary) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA056u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA056u, 0);
    st.dictionaries.classList.push_back({0x21000011u, "FTML::DC::Configuration", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x21000011u, 1);
    st.dictionaries.classList.push_back({0x499602DDu, "FTMLUtil::FTML::NamedValueTable", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x499602DDu, 2);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLUtil", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA056u, 0x93u);
    push_string("XMSE-System");
    push_string("Dark");
    push_pointer_header(0x21000011u, 0x94u);
    push_string("SYS");
    push_string("comment-text");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 27;
    const std::uint8_t hour = 17;
    const std::uint8_t minute = 25;
    const std::uint16_t millisecond = 456;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, millisecond);
    push_string("base-name");
    push_pointer_header(0x499602DDu, 0x95u);
    push_string("ConfigApp");
    push_pointer_header(0x21000011u, 0x96u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr || config_set->entries.empty() || !config_set->entries[0].configuration.has_value()){
        return;
    }
    const auto& summary = *config_set->entries[0].configuration;
    CHECK(summary.hasConfigInfo);
    CHECK(summary.configInfoType.class_name == "FTMLUtil::FTML::NamedValueTable");
    CHECK(summary.configInfoObjectId == 0x95u);
    CHECK(summary.configInfoBodyOffset != 0);
    CHECK(summary.configInfoSummary.has_value());
    if(!summary.configInfoSummary.has_value()){
        return;
    }
    CHECK(summary.configInfoSummary->entries.size() == 1);
    if(summary.configInfoSummary->entries.size() != 1){
        return;
    }
    const auto& entry = summary.configInfoSummary->entries[0];
    CHECK(entry.key == "ConfigApp");
    CHECK(entry.ordinal == 0);
    CHECK(entry.type.class_name == "FTML::DC::Configuration");
    CHECK(entry.object_id == 0x96u);
    CHECK(entry.body_offset != 0);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsLegacyRootNamedValueSetSummary) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA052u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA052u, 0);
    st.dictionaries.classList.push_back({0x499602DDu, "FTMLUtil::FTML::NamedValueTable", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x499602DDu, 1);
    st.dictionaries.classList.push_back({0x23000033u, "FTMLUtil::FTML::NamedValueSet", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000033u, 2);
    st.dictionaries.classList.push_back({0x24000044u, "FTML::XMSE::Configuration", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x24000044u, 3);
    st.dictionaries.classList.push_back({0x22000022u, "FTML::DC::ConfigInfo", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x22000022u, 4);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLUtil", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA052u, 0xA0u);
    push_string("XMSE-System");
    push_pointer_header(0x23000033u, 0xA1u);
    push_string("Sample");
    push_pointer_header(0x24000044u, 0xA2u);
    push_string("SYS");
    push_string("legacy-comment");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 27;
    const std::uint8_t hour = 18;
    const std::uint8_t minute = 2;
    const std::uint16_t millisecond = 789;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, millisecond);
    push_string("legacy-base");
    push_pointer_header(0x22000022u, 0xA3u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    const std::uint32_t sums_per_cycle = 200;
    const std::uint32_t timing_mode = 7;
    const std::uint32_t saturation = 1234;
    const std::uint32_t turns_per_cycle0 = 55;
    const std::uint32_t turns_per_cycle1 = 66;
    const std::uint32_t num_pixel = 2048;
    push_scalar(TypeCode::ID::UInt32, sums_per_cycle);
    push_scalar(TypeCode::ID::UInt32, timing_mode);
    push_scalar(TypeCode::ID::UInt32, saturation);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    push_scalar(TypeCode::ID::UInt32, turns_per_cycle0);
    push_scalar(TypeCode::ID::UInt32, turns_per_cycle1);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));
    push_scalar(TypeCode::ID::UInt32, num_pixel);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->systemID == "XMSE-System");
    CHECK(config_set->hasLegacyRoot);
    CHECK(config_set->legacyRoot.type.class_name == "FTMLUtil::FTML::NamedValueSet");
    CHECK(config_set->legacyRoot.expected.class_name == "FTMLUtil::FTML::NamedValueTable");
    CHECK(config_set->legacyRoot.compatibility == FTML::SmartPointer::Compatibility::Different);
    CHECK(config_set->legacyRoot.object_id == 0xA1u);
    CHECK(config_set->legacyRoot.body_offset != 0);
    CHECK(config_set->legacyRoot.namedValueSet.has_value());
    if(!config_set->legacyRoot.namedValueSet.has_value()){
        return;
    }
    CHECK(config_set->legacyRoot.namedValueSet->entries.size() == 1);
    if(config_set->legacyRoot.namedValueSet->entries.size() != 1){
        return;
    }
    const auto& entry = config_set->legacyRoot.namedValueSet->entries[0];
    CHECK(entry.key == "Sample");
    CHECK(entry.type.class_name == "FTML::XMSE::Configuration");
    CHECK(entry.expected.class_id == 0);
    CHECK(entry.compatibility == FTML::SmartPointer::Compatibility::NoExpectation);
    CHECK(entry.object_id == 0xA2u);
    CHECK(entry.body_offset != 0);
    CHECK(entry.configuration.has_value());
    if(!entry.configuration.has_value()){
        return;
    }
    CHECK(entry.configuration->system == "SYS");
    CHECK(entry.configuration->comment == "legacy-comment");
    CHECK(entry.configuration->hasTimestamp);
    CHECK(entry.configuration->timestampYear == 2026);
    CHECK(entry.configuration->timestampMonth == 6);
    CHECK(entry.configuration->timestampDay == 27);
    CHECK(entry.configuration->timestampHour == 18);
    CHECK(entry.configuration->timestampMinute == 2);
    CHECK(entry.configuration->timestampSubMinuteRaw == 789);
    CHECK(entry.configuration->baseName == "legacy-base");
    CHECK(entry.configuration->hasConfigInfo);
    CHECK(entry.configuration->configInfoType.class_name == "FTML::DC::ConfigInfo");
    CHECK(entry.configuration->configInfoObjectId == 0xA3u);
    CHECK(entry.configuration->configInfoBodyOffset != 0);
    CHECK(entry.configuration->hasXMSETail);
    CHECK(entry.configuration->sumsPerCycle == sums_per_cycle);
    CHECK(entry.configuration->timingMode == timing_mode);
    CHECK(entry.configuration->saturation == saturation);
    CHECK(entry.configuration->turnsPerCycle0 == turns_per_cycle0);
    CHECK(entry.configuration->turnsPerCycle1 == turns_per_cycle1);
    CHECK(entry.configuration->numPixel == num_pixel);
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsGroupedNameStringBaseName) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA053u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA053u, 0);
    st.dictionaries.classList.push_back({0x21000033u, "FTML::DC::Configuration", 3, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x21000033u, 1);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA053u, 0xB0u);
    push_string("XMSE-System");
    push_string("Dark");
    push_pointer_header(0x21000033u, 0xB1u);
    push_string("SYS");
    push_string("comment-text");
    const std::uint16_t year = 2026;
    const std::uint8_t month = 6;
    const std::uint8_t day = 27;
    const std::uint8_t hour = 19;
    const std::uint8_t minute = 15;
    const std::uint16_t millisecond = 321;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, millisecond);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    push_string("grouped-base");
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->entries.size() == 1);
    if(config_set->entries.size() != 1){
        return;
    }
    CHECK(config_set->entries[0].configuration.has_value());
    if(!config_set->entries[0].configuration.has_value()){
        return;
    }
    CHECK(config_set->entries[0].configuration->baseName == "grouped-base");
    CHECK(st.input.offset == st.input.buffer.size());
}

TEST_CASE(XMSE_ReadDynamic_ConfigurationSetReadsNamedValueListRawSummary) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA055u, "FTML::XMSE::ConfigurationSet", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA055u, 0);
    st.dictionaries.classList.push_back({0x499602DDu, "FTMLUtil::FTML::NamedValueTable", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x499602DDu, 1);
    st.dictionaries.classList.push_back({0x23000033u, "FTMLUtil::FTML::NamedValueSet", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000033u, 2);
    st.dictionaries.classList.push_back({0x23000044u, "FTMLUtil::FTML::NamedValueList", 1, 0x55667799u});
    st.dictionaries.classIndexById.emplace(0x23000044u, 3);
    st.dictionaries.classList.push_back({0x22000022u, "FTML::DC::ConfigInfo", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x22000022u, 4);
    st.dictionaries.classList.push_back({0x23000055u, "FTML::XMSE::Configuration", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0x23000055u, 5);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);
    st.dictionaries.moduleList.push_back({0x55667799u, "FTMLUtil", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667799u, 1);

    auto push_string = [&](std::string_view text, std::string_view name = {}) {
        std::uint8_t header = static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80;
        if(!name.empty()){
            header |= 0x40;
        }
        st.input.buffer.push_back(header);
        if(!name.empty()){
            st.input.buffer.push_back(static_cast<std::uint8_t>(name.size()));
            st.input.buffer.insert(st.input.buffer.end(), name.begin(), name.end());
        }
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA055u, 0xC0u);
    push_string("XMSE-System");
    push_pointer_header(0x23000033u, 0xC1u);
    push_string("TF");
    push_pointer_header(0x23000044u, 0xC2u);
    const std::int8_t entries = 1;
    push_scalar(TypeCode::ID::Int8, entries);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    push_string("@Default");
    const std::uint8_t value_tag = 0x20u;
    push_scalar(TypeCode::ID::UInt8, value_tag);
    push_pointer_header(0x23000055u, 0xC3u);
    push_string("NVL-SYS");
    push_string("nvl-comment");
    const std::uint16_t year = 2024;
    const std::uint8_t month = 5;
    const std::uint8_t day = 9;
    const std::uint8_t hour = 11;
    const std::uint8_t minute = 47;
    const std::uint16_t sub_minute = 321;
    push_scalar(TypeCode::ID::UInt16, year);
    push_scalar(TypeCode::ID::UChar, month);
    push_scalar(TypeCode::ID::UChar, day);
    push_scalar(TypeCode::ID::UChar, hour);
    push_scalar(TypeCode::ID::UChar, minute);
    push_scalar(TypeCode::ID::UInt16, sub_minute);
    push_string("nvl-base");
    push_pointer_header(0x22000022u, 0xC4u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    const std::uint32_t sums_per_cycle = 300;
    const std::uint32_t timing_mode = 9;
    const std::uint32_t saturation = 777;
    const std::uint32_t turns_per_cycle0 = 88;
    const std::uint32_t turns_per_cycle1 = 99;
    const std::uint32_t num_pixel = 4096;
    push_scalar(TypeCode::ID::UInt32, sums_per_cycle);
    push_scalar(TypeCode::ID::UInt32, timing_mode);
    push_scalar(TypeCode::ID::UInt32, saturation);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginGroup));
    push_scalar(TypeCode::ID::UInt32, turns_per_cycle0);
    push_scalar(TypeCode::ID::UInt32, turns_per_cycle1);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));
    push_scalar(TypeCode::ID::UInt32, num_pixel);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndGroup));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* config_set = result.as<FTML::XMSE::ConfigurationSet>();
    CHECK(config_set != nullptr);
    if(config_set == nullptr){
        return;
    }
    CHECK(config_set->hasLegacyRoot);
    CHECK(config_set->legacyRoot.namedValueSet.has_value());
    if(!config_set->legacyRoot.namedValueSet.has_value()){
        return;
    }
    CHECK(config_set->legacyRoot.namedValueSet->entries.size() == 1);
    if(config_set->legacyRoot.namedValueSet->entries.size() != 1){
        return;
    }
    const auto& entry = config_set->legacyRoot.namedValueSet->entries[0];
    CHECK(entry.key == "TF");
    CHECK(entry.type.class_name == "FTMLUtil::FTML::NamedValueList");
    CHECK(entry.namedValueList.has_value());
    if(!entry.namedValueList.has_value()){
        return;
    }
    CHECK(entry.namedValueList->itemCount >= 2);
    CHECK(entry.namedValueList->hasEntriesCount);
    CHECK(entry.namedValueList->entriesCountRaw == 1);
    CHECK(entry.namedValueList->structuredEntries.size() == 1);
    if(entry.namedValueList->structuredEntries.size() != 1){
        return;
    }
    const auto& structured = entry.namedValueList->structuredEntries[0];
    CHECK(structured.name == "@Default");
    CHECK(structured.hasValueTag);
    CHECK(structured.valueTag == 0x20u);
    CHECK(structured.pointerType.class_name == "FTML::XMSE::Configuration");
    CHECK(structured.expected.class_id == 0);
    CHECK(structured.compatibility == FTML::SmartPointer::Compatibility::NoExpectation);
    CHECK(structured.object_id == 0xC3u);
    CHECK(structured.body_offset != 0);
    CHECK(structured.configuration.has_value());
    if(!structured.configuration.has_value()){
        return;
    }
    CHECK(structured.configuration->system == "NVL-SYS");
    CHECK(structured.configuration->comment == "nvl-comment");
    CHECK(structured.configuration->hasTimestamp);
    CHECK(structured.configuration->timestampYear == 2024);
    CHECK(structured.configuration->timestampMonth == 5);
    CHECK(structured.configuration->timestampDay == 9);
    CHECK(structured.configuration->timestampHour == 11);
    CHECK(structured.configuration->timestampMinute == 47);
    CHECK(structured.configuration->timestampSubMinuteRaw == 321);
    CHECK(structured.configuration->baseName == "nvl-base");
    CHECK(structured.configuration->hasConfigInfo);
    CHECK(structured.configuration->configInfoType.class_name == "FTML::DC::ConfigInfo");
    CHECK(structured.configuration->configInfoObjectId == 0xC4u);
    CHECK(structured.configuration->configInfoBodyOffset != 0);
    CHECK(structured.configuration->hasXMSETail);
    CHECK(structured.configuration->sumsPerCycle == sums_per_cycle);
    CHECK(structured.configuration->timingMode == timing_mode);
    CHECK(structured.configuration->saturation == saturation);
    CHECK(structured.configuration->turnsPerCycle0 == turns_per_cycle0);
    CHECK(structured.configuration->turnsPerCycle1 == turns_per_cycle1);
    CHECK(structured.configuration->numPixel == num_pixel);
    bool saw_entries = false;
    bool saw_group = false;
    bool saw_configuration = false;
    for(const auto& preview : entry.namedValueList->preview){
        saw_entries = saw_entries || preview.find("value=int8(1)") != std::string::npos;
        saw_group = saw_group || preview.find("group[") != std::string::npos;
        saw_configuration = saw_configuration || preview.find("@Default") != std::string::npos
            || preview.find("Configuration") != std::string::npos;
    }
    CHECK(saw_entries);
    CHECK(saw_group);
    CHECK(saw_configuration);
}

TEST_CASE(XMSE_ReadDynamic_DPRecipeReadsAnonymousConfigAppPrefix) {
    BinaryArchive ar;
    auto& st = ar.archive().state();
    st.dictionaries.classList.push_back({0xDCAAA054u, "FTML::XMSE::DPRecipe", 1, 0x55667788u});
    st.dictionaries.classIndexById.emplace(0xDCAAA054u, 0);
    st.dictionaries.moduleList.push_back({0x55667788u, "FTMLSysXMSE", "7.00.14"});
    st.dictionaries.moduleIndexById.emplace(0x55667788u, 0);

    auto push_string = [&](std::string_view text) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::Char) | 0x80);
        st.input.buffer.push_back(0x01);
        st.input.buffer.push_back(static_cast<std::uint8_t>(text.size()));
        st.input.buffer.insert(st.input.buffer.end(), text.begin(), text.end());
    };
    auto push_bool = [&](bool value) {
        const std::uint8_t raw = value ? 1u : 0u;
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::Bool));
        st.input.buffer.push_back(raw);
    };
    auto push_scalar = [&](TypeCode::ID id, auto value) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(id));
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
        st.input.buffer.insert(st.input.buffer.end(), bytes, bytes + sizeof(value));
    };
    auto push_pointer_header = [&](std::uint32_t class_id, std::uint32_t object_id) {
        st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::BeginPointer));
        st.input.buffer.push_back(0x44);
        const std::array header{
            static_cast<std::uint8_t>(class_id & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((class_id >> 24) & 0xFFu),
            static_cast<std::uint8_t>(object_id & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 8) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 16) & 0xFFu),
            static_cast<std::uint8_t>((object_id >> 24) & 0xFFu),
        };
        st.input.buffer.insert(st.input.buffer.end(), header.begin(), header.end());
    };

    st.input.buffer.clear();
    push_pointer_header(0xDCAAA054u, 0xC0u);
    push_string("@Default");
    push_pointer_header(0x17000001u, 0xC1u);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));
    const std::int32_t binning = 2;
    push_scalar(TypeCode::ID::Int32, binning);
    push_bool(true);
    push_bool(false);
    push_bool(true);
    push_bool(false);
    push_bool(true);
    push_bool(false);
    push_bool(true);
    push_bool(false);
    push_bool(true);
    st.input.buffer.push_back(static_cast<std::uint8_t>(TypeCode::ID::EndPointer));

    st.input.offset = 0;
    st.input.finalized = true;
    st.parsing.itemHeader = {};

    FTML::XMSE::DynamicObjectReadResult result{};
    CHECK(FTML::XMSE::read_dynamic(ar, result));
    CHECK(result.status == FTML::XMSE::DynamicReadStatus::Handled);
    const auto* recipe = result.as<FTML::XMSE::DPRecipe>();
    CHECK(recipe != nullptr);
    if(recipe == nullptr){
        return;
    }
    CHECK(recipe->configApp == "@Default");
    CHECK(recipe->binning == binning);
    CHECK(recipe->applyMultiScanErr);
    CHECK(!recipe->applyPSF);
    CHECK(recipe->applyLinearity);
    CHECK(!recipe->applyDCOffset);
    CHECK(recipe->applyA0P0Offset);
    CHECK(!recipe->applyWShift);
    CHECK(recipe->applyTilt);
    CHECK(!recipe->applyIDN);
    CHECK(recipe->modelTilt);
    CHECK(st.input.offset == st.input.buffer.size());
}


int run_unit_tests() {
    for(const auto& test : registry()){
        test.fn();
    }
    if(state().failed != 0){
        std::cout << std::format("[TEST] failed={}", state().failed) << "\n";
        return 1;
    }
    std::cout << "[TEST] ok\n";
    return 0;
}
