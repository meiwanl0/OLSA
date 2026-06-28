#include "../include/BinaryArchive.h"
#include "../include/BinaryFileReader.h"
#include "../include/XMSERawDataSet.h"
#include "../include/protocol/ByteUtil.h"

#include <format>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RawDataSlotIds {
    std::uint64_t ref{};
    std::uint64_t dark{};
    std::uint64_t sample{};
};

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

    const auto& state = archive.archive().state();
    const auto it = state.dictionaries.classIndexById.find(class_id);
    if(it == state.dictionaries.classIndexById.end()){
        return false;
    }
    return state.dictionaries.classList[it->second].name == expected_name;
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

[[nodiscard]] bool read_formatted_uint32_array(BinaryArchive& archive) {
    const auto header = archive.next();
    if(!header.hasItem || header.signTypeCode != TypeCode::ID::UInt32 || header.payloadType != TypeCode::ID::UInt32){
        return false;
    }
    if(header.payloadBytes.empty() || header.payloadBytes.size() % sizeof(std::uint32_t) != 0){
        return false;
    }

    const auto count = header.payloadBytes.size() / sizeof(std::uint32_t);
    return archive.get(TypeCode::ID::UInt32, count);
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
        return read_formatted_uint32_array(archive) && read_expected_item(archive, TypeCode::ID::EndPointer);
    }
    return false;
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

[[nodiscard]] RawDataSlotIds inspect_rawdata_slot_ids(BinaryArchive& archive) {
    RawDataSlotIds ids{};

    for(std::size_t index = 0; index <= 20; ++index){
        const auto header = archive.next();
        if(!header.hasItem){
            break;
        }
        if(header.signTypeCode == TypeCode::ID::EndPointer){
            archive.get(TypeCode::ID::EndPointer, 1);
            break;
        }
        if(index == 17 && header.signTypeCode == TypeCode::ID::BeginPointer){
            ids.ref = header.tempObjectId;
        }
        if(index == 18 && header.signTypeCode == TypeCode::ID::BeginPointer){
            ids.dark = header.tempObjectId;
        }
        if(index == 19 && header.signTypeCode == TypeCode::ID::BeginPointer){
            ids.sample = header.tempObjectId;
        }
        if(!skip_item_quiet(archive)){
            break;
        }
    }

    return ids;
}

[[nodiscard]] BinaryArchive make_node_archive(BinaryFileReader& reader, std::size_t node_index) {
    BinaryArchive archive;
    archive.reset(reader.read_data_block(node_index));
    archive.read_first_section();
    return archive;
}

[[nodiscard]] std::string role_name(std::uint64_t object_id, const RawDataSlotIds& ids) {
    if(object_id == ids.ref){
        return "Ref";
    }
    if(object_id == ids.dark){
        return "Dark";
    }
    if(object_id == ids.sample){
        return "Sample";
    }
    return std::format("Unknown(oid={})", object_id);
}

void dump_array(
    std::ofstream& out,
    std::string_view role,
    std::string_view field_name,
    const std::optional<FTML::XMSE::RawData::UInt32ArrayPayload>& payload) {
    out << field_name << ": ";
    if(!payload.has_value()){
        out << "<absent>\n";
        return;
    }

    out << "class_id=0x" << std::hex << payload->class_id << std::dec
        << " object_id=" << payload->object_id
        << " kind=" << (payload->is_2d ? "Array2D<unsigned>" : "Array<unsigned>");
    if(payload->is_2d){
        out << " dims=" << payload->dim0 << "x" << payload->dim1;
    } else {
        out << " count=" << payload->element_count();
    }
    out << "\n";

    out << role << "." << field_name << ".values=[";
    for(std::size_t i = 0; i < payload->values.size(); ++i){
        if(i != 0){
            out << ", ";
        }
        out << payload->values[i];
    }
    out << "]\n";
}
}  // namespace

int main(int argc, char** argv) {
    if(argc < 3){
        std::cerr << "usage: extract_node3_uint32_arrays <test.dat> <output.txt>\n";
        return 1;
    }

    BinaryFileReader reader(argv[1]);
    if(!reader.read_all()){
        std::cerr << "failed to read archive file\n";
        return 1;
    }
    if(reader.get_nodes().size() <= 3){
        std::cerr << "node[3] not available\n";
        return 1;
    }

    BinaryArchive slot_archive = make_node_archive(reader, 3);
    const RawDataSlotIds slot_ids = inspect_rawdata_slot_ids(slot_archive);

    BinaryArchive scan_archive = make_node_archive(reader, 3);
    const auto results = FTML::XMSE::collect_supported_dynamic_objects(scan_archive);

    std::ofstream out(argv[2], std::ios::binary);
    if(!out){
        std::cerr << "failed to open output file\n";
        return 1;
    }

    out << "node[3] uint32 arrays\n";
    out << "slot_object_ids: ref=" << slot_ids.ref
        << " dark=" << slot_ids.dark
        << " sample=" << slot_ids.sample << "\n\n";

    for(const auto& result : results){
        if(result.kind != FTML::XMSE::DynamicObjectKind::RawData){
            continue;
        }
        const auto* value = result.as<FTML::XMSE::RawData>();
        if(value == nullptr){
            continue;
        }

        const auto role = role_name(result.object_id, slot_ids);
        out << role << ":\n";
        out << "object_id=" << result.object_id << " body_offset=0x" << std::hex << result.body_offset << std::dec << "\n";
        dump_array(out, role, "sig", value->sig);
        dump_array(out, role, "enc1", value->enc1);
        dump_array(out, role, "enc2", value->enc2);
        dump_array(out, role, "clk", value->clk);
        dump_array(out, role, "bm", value->bm);
        out << "\n";
    }

    return 0;
}
