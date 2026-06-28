#include "../include/BinaryArchive.h"

#include "../include/protocol/ByteUtil.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

namespace {
[[nodiscard]] bool read_bytes(ArchiveState& state, std::span<std::uint8_t> out) {
    if(state.input.offset + out.size() > state.input.buffer.size()){
        return false;
    }
    std::memcpy(out.data(), state.input.buffer.data() + state.input.offset, out.size());
    state.input.offset += static_cast<std::uint32_t>(out.size());
    if(state.input.counter){
        state.input.counter->add(out.size());
    }
    return true;
}

[[nodiscard]] bool read_bytes(ArchiveState& state, std::vector<std::uint8_t>& out, std::size_t size) {
    out.resize(size);
    return read_bytes(state, std::span<std::uint8_t>(out.data(), out.size()));
}

[[nodiscard]] bool read_u8(ArchiveState& state, std::uint8_t& out) {
    if(state.input.offset >= state.input.buffer.size()){
        return false;
    }
    out = state.input.buffer[state.input.offset++];
    if(state.input.counter){
        state.input.counter->add(1);
    }
    return true;
}

[[nodiscard]] bool read_typed_block(ArchiveState& state, void* dst, std::size_t type_size, std::size_t count) {
    const auto total = type_size * count;
    if(total == 0){
        return true;
    }
    if(state.input.offset + total > state.input.buffer.size()){
        return false;
    }
    std::memcpy(dst, state.input.buffer.data() + state.input.offset, total);
    state.input.offset += static_cast<std::uint32_t>(total);
    if(state.input.counter){
        state.input.counter->add(total);
    }
    return true;
}

[[nodiscard]] bool read_metadata_bytes(ArchiveState& state, std::vector<std::uint8_t>& meta_bytes, std::uint8_t meta_len) {
    meta_bytes.resize(static_cast<std::size_t>(meta_len) + 1);
    if(!read_typed_block(state, meta_bytes.data(), 1, meta_len)){
        return false;
    }
    meta_bytes[meta_len] = 0;
    return true;
}

[[nodiscard]] bool read_object_pointer_info(
    ArchiveState& state,
    std::uint32_t& out_class_id,
    std::uint64_t& out_object_id) {
    std::uint8_t read_char{};
    if(!read_u8(state, read_char)){
        return false;
    }
    if(read_char == 0){
        return true;
    }

    const auto module_len = static_cast<std::uint8_t>(read_char >> 4);
    const auto class_len = static_cast<std::uint8_t>(read_char & 0x0F);
    out_class_id = 0;
    out_object_id = 0;

    if(class_len != 0 && !read_typed_block(state, &out_class_id, 1, class_len)){
        return false;
    }
    if(module_len != 0 && !read_typed_block(state, &out_object_id, 1, module_len)){
        return false;
    }
    return true;
}

[[nodiscard]] bool read_format_primitive_array(
    ArchiveState& state,
    TypeCode::ID type_code,
    std::vector<std::uint8_t>& out_payload) {
    out_payload.clear();
    const auto type_size = TypeCode::size(static_cast<std::uint8_t>(type_code));
    if(type_size == 0){
        return false;
    }

    std::uint8_t format_header{};
    if(!read_u8(state, format_header)){
        return false;
    }
    if(format_header == 0){
        return true;
    }

    const auto packed_len_size = static_cast<std::uint8_t>(format_header >> 4);
    const auto count_size = static_cast<std::uint8_t>(format_header & 0x0F);

    std::uint32_t count = 0;
    if(count_size != 0){
        if(state.input.offset + count_size > state.input.buffer.size()){
            return false;
        }
        count = read_little_endian<std::uint32_t>(
            std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, count_size));
        state.input.offset += count_size;
    }

    std::uint32_t packed_len = 0;
    if(packed_len_size != 0){
        if(state.input.offset + packed_len_size > state.input.buffer.size()){
            return false;
        }
        packed_len = read_little_endian<std::uint32_t>(
            std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, packed_len_size));
        state.input.offset += packed_len_size;
    }

    if(count == 0){
        return false;
    }

    const auto total = static_cast<std::size_t>(type_size) * count;
    if(packed_len == 0){
        return read_bytes(state, out_payload, total);
    }

    if(state.input.offset + packed_len > state.input.buffer.size()){
        return false;
    }

    const auto src = std::span<const std::uint8_t>(state.input.buffer.data() + state.input.offset, packed_len);
    std::vector<std::uint8_t> unpacked;
    if(!state.parsing.compress.unpack(src, unpacked)){
        return false;
    }
    if(unpacked.size() != total){
        return false;
    }

    state.input.offset += packed_len;
    out_payload = std::move(unpacked);
    return true;
}

[[nodiscard]] bool is_marker_type(TypeCode::ID code) {
    const auto raw = static_cast<std::uint8_t>(code);
    return raw >= static_cast<std::uint8_t>(TypeCode::ID::BeginGroup) &&
           raw <= static_cast<std::uint8_t>(TypeCode::ID::EndPointer);
}

[[nodiscard]] bool is_primitive_type(TypeCode::ID code) {
    const auto raw = static_cast<std::uint8_t>(code);
    return raw > 0 && raw < 0x13;
}

[[nodiscard]] bool can_convert_payload(const ItemHeader& header, TypeCode::ID hope_code) {
    return is_primitive_type(hope_code) && is_primitive_type(header.payloadType) && !header.payloadBytes.empty();
}

[[nodiscard]] const ArchiveUtil::ClassItem* find_class_item(const ArchiveState& state, std::uint32_t class_id) {
    const auto it = state.dictionaries.classIndexById.find(class_id);
    if(it == state.dictionaries.classIndexById.end()){
        return nullptr;
    }
    return &state.dictionaries.classList[it->second];
}

[[nodiscard]] const ArchiveUtil::ModuleItem* find_module_item(const ArchiveState& state, std::uint32_t module_id) {
    const auto it = state.dictionaries.moduleIndexById.find(module_id);
    if(it == state.dictionaries.moduleIndexById.end()){
        return nullptr;
    }
    return &state.dictionaries.moduleList[it->second];
}

[[nodiscard]] OLSA::Container::ResolvedTypeInfo resolve_type_info(
    const ArchiveState& state,
    std::uint32_t class_id,
    bool is_pointer,
    bool from_tag) {
    OLSA::Container::ResolvedTypeInfo info{};
    info.class_id = class_id;
    info.is_pointer = is_pointer;
    info.from_tag = from_tag;

    const auto* class_item = find_class_item(state, class_id);
    if(class_item == nullptr){
        return info;
    }

    info.known = true;
    info.class_name = class_item->name;
    info.class_version = class_item->version;
    info.module_id = class_item->module_id;

    if(const auto* module_item = find_module_item(state, class_item->module_id); module_item != nullptr){
        info.module_name = module_item->name;
        info.module_version = module_item->version;
    }
    return info;
}

void clear_resolved_type(ArchiveState& state) {
    state.parsing.resolvedType.reset();
}

[[nodiscard]] bool remember_resolved_pointer_type(ArchiveState& state, const ItemHeader& header) {
    if(header.signTypeCode != TypeCode::ID::BeginPointer){
        return false;
    }

    bool from_tag = false;
    std::uint32_t class_id = header.tempClassId;
    if(header.hasTag){
        const auto it = state.dictionaries.taggedObjectIndex.find(header.tag);
        if(it != state.dictionaries.taggedObjectIndex.end()){
            class_id = it->second;
            from_tag = true;
        } else if(class_id != 0) {
            state.dictionaries.taggedObjectIndex.emplace(header.tag, class_id);
        }
    }

    if(class_id == 0){
        return false;
    }

    state.parsing.resolvedType = resolve_type_info(state, class_id, true, from_tag);
    return true;
}

[[nodiscard]] bool remember_resolved_object_type(ArchiveState& state, const ItemHeader& header) {
    if(header.signTypeCode != TypeCode::ID::BeginObject || header.tempClassId == 0){
        return false;
    }

    state.parsing.resolvedType = resolve_type_info(state, header.tempClassId, false, false);
    return true;
}

class BinaryArchiveEngine final : public IArchiveEngine {
public:
    bool get(ArchiveState& state, TypeCode::ID hopeCode, std::size_t hopeReadSize) override {
        if(!doGet(state, hopeCode)){
            return false;
        }

        auto& header = state.parsing.itemHeader;
        const auto type_size = TypeCode::size(static_cast<std::uint8_t>(hopeCode));
        if(type_size == 0 || hopeReadSize == 0){
            return true;
        }

        if(header.payloadType != hopeCode){
            return false;
        }

        const auto total = type_size * hopeReadSize;
        return header.payloadBytes.size() >= total;
    }

    bool gets(ArchiveState& state, std::string& str, std::size_t maxSize) override {
        if(!doGet(state, TypeCode::ID::None)){
            return false;
        }

        const auto& header = state.parsing.itemHeader;
        if(header.payloadType != TypeCode::ID::Char && header.payloadType != TypeCode::ID::UChar){
            return false;
        }
        if(maxSize != 0 && header.payloadBytes.size() > maxSize){
            return false;
        }

        str.assign(reinterpret_cast<const char*>(header.payloadBytes.data()), header.payloadBytes.size());
        if(!str.empty() && str.back() == '\0'){
            str.pop_back();
        }
        return true;
    }

    void getClassAndModuleList(ArchiveState& state) override {
        state.dictionaries.classList.clear();
        state.dictionaries.moduleList.clear();
        state.dictionaries.classIndexById.clear();
        state.dictionaries.moduleIndexById.clear();

        if(!get(state, TypeCode::ID::BeginGroup, 1)){
            return;
        }
        if(!doGet(state, TypeCode::ID::None)){
            return;
        }
        const auto num_class = static_cast<std::size_t>(
            read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes)));
        state.dictionaries.classList.reserve(num_class);

        for([[maybe_unused]] const auto i : std::views::iota(std::size_t{0}, num_class)){
            ArchiveUtil::ClassItem item{};
            if(!get(state, TypeCode::ID::BeginGroup, 1)){
                break;
            }
            if(!get(state, TypeCode::ID::UInt32, 1)){
                break;
            }
            item.class_id = read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes));
            if(!gets(state, item.name, 0x78)){
                break;
            }
            if(!get(state, TypeCode::ID::Int16, 1)){
                break;
            }
            item.version = read_little_endian<std::uint16_t>(as_bytes(state.parsing.itemHeader.payloadBytes));
            if(!get(state, TypeCode::ID::UInt32, 1)){
                break;
            }
            item.module_id = read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes));
            state.dictionaries.classIndexById.emplace(item.class_id, state.dictionaries.classList.size());
            state.dictionaries.classList.push_back(std::move(item));
            if(!get(state, TypeCode::ID::EndGroup, 1)){
                break;
            }
        }

        if(!get(state, TypeCode::ID::EndGroup, 1)){
            return;
        }
        if(!get(state, TypeCode::ID::BeginGroup, 1)){
            return;
        }
        if(!doGet(state, TypeCode::ID::None)){
            return;
        }

        const auto num_module = static_cast<std::size_t>(
            read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes)));
        state.dictionaries.moduleList.reserve(num_module);

        for([[maybe_unused]] const auto i : std::views::iota(std::size_t{0}, num_module)){
            ArchiveUtil::ModuleItem item{};
            if(!get(state, TypeCode::ID::BeginGroup, 1)){
                break;
            }
            if(!get(state, TypeCode::ID::UInt32, 1)){
                break;
            }
            item.module_id = read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes));
            if(!gets(state, item.name, 0x78)){
                break;
            }
            if(!gets(state, item.version, 0x78)){
                break;
            }
            state.dictionaries.moduleIndexById.emplace(item.module_id, state.dictionaries.moduleList.size());
            state.dictionaries.moduleList.push_back(std::move(item));
            if(!get(state, TypeCode::ID::EndGroup, 1)){
                break;
            }
        }

        get(state, TypeCode::ID::EndGroup, 1);
    }

    bool getPointer(ArchiveState& state) override {
        clear_resolved_type(state);
        const auto header = next(state);
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode != TypeCode::ID::BeginPointer){
            return false;
        }

        const auto resolved = remember_resolved_pointer_type(state, header);
        state.parsing.itemHeader.hasItem = false;
        return resolved;
    }

    bool getObject(ArchiveState& state) override {
        clear_resolved_type(state);
        const auto header = next(state);
        if(!header.hasItem){
            return false;
        }
        if(header.signTypeCode != TypeCode::ID::BeginObject){
            return false;
        }

        const auto resolved = remember_resolved_object_type(state, header);
        state.parsing.itemHeader.hasItem = false;
        return resolved;
    }

    bool check_version(ArchiveState&) override {
        return true;
    }

    bool doGet(ArchiveState& state, TypeCode::ID hopeCode) override {
        if(!state.input.finalized){
            return false;
        }

        auto& header = state.parsing.itemHeader;
        if(!header.hasItem){
            next(state);
        }
        if(!header.hasItem){
            return false;
        }

        if(hopeCode != TypeCode::ID::None && header.signTypeCode != hopeCode){
            if(!can_convert_payload(header, hopeCode)){
                return false;
            }

            const auto src_size = TypeCode::size(static_cast<std::uint8_t>(header.payloadType));
            const auto dst_size = TypeCode::size(static_cast<std::uint8_t>(hopeCode));
            if(src_size == 0 || dst_size == 0 || header.payloadBytes.size() % src_size != 0){
                return false;
            }

            const auto count = header.payloadBytes.size() / src_size;
            std::vector<std::uint8_t> converted(dst_size * count);
            if(!TypeCode::convert(hopeCode, converted.data(), header.payloadType, header.payloadBytes.data(), count)){
                return false;
            }
            header.payloadBytes = std::move(converted);
            header.payloadType = hopeCode;
        }

        if(hopeCode == TypeCode::ID::None && !is_primitive_type(header.signTypeCode) && !is_marker_type(header.signTypeCode)){
            return false;
        }

        header.hasItem = false;
        return true;
    }

    ItemHeader next(ArchiveState& state) override {
        auto& header = state.parsing.itemHeader;
        if(!state.input.finalized || header.hasItem){
            return header;
        }

        header = {};
        if(state.input.offset >= state.input.buffer.size()){
            return header;
        }

        std::uint8_t read_char{};
        if(!read_u8(state, read_char)){
            return header;
        }

        const auto read_type_code = static_cast<std::uint8_t>(read_char & 0x3F);
        const auto has_meta = (read_char & 0x40) != 0;
        const auto is_format = (read_char & 0x80) != 0;

        if(has_meta){
            std::uint8_t meta_len{};
            if(!read_u8(state, meta_len) || !read_metadata_bytes(state, header.metaBytes, meta_len)){
                header = {};
                return header;
            }
        }

        header.signTypeCode = static_cast<TypeCode::ID>(read_type_code);
        switch(header.signTypeCode){
            case TypeCode::ID::Bool:
            case TypeCode::ID::Char:
            case TypeCode::ID::SChar:
            case TypeCode::ID::Int8:
            case TypeCode::ID::Int16:
            case TypeCode::ID::Int32:
            case TypeCode::ID::Int64:
            case TypeCode::ID::UChar:
            case TypeCode::ID::UInt8:
            case TypeCode::ID::UInt16:
            case TypeCode::ID::UInt32:
            case TypeCode::ID::UInt64:
            case TypeCode::ID::Float:
            case TypeCode::ID::Double:
            case TypeCode::ID::LDouble:
            case TypeCode::ID::FloatComplex:
            case TypeCode::ID::DoubleComplex:
            case TypeCode::ID::LongComplex: {
                const auto type_size = TypeCode::size(read_type_code);
                if(type_size == 0){
                    header = {};
                    return header;
                }
                if(is_format){
                    if(!read_format_primitive_array(state, header.signTypeCode, header.payloadBytes)){
                        header = {};
                        return header;
                    }
                }else{
                    if(!read_bytes(state, header.payloadBytes, type_size)){
                        header = {};
                        return header;
                    }
                }
                header.payloadType = header.signTypeCode;
                break;
            }
            case TypeCode::ID::BeginGroup:
            case TypeCode::ID::EndGroup:
            case TypeCode::ID::EndObject:
            case TypeCode::ID::EndPointer:
                break;
            case TypeCode::ID::BeginObject:
            case TypeCode::ID::BeginPointer:
                if(!read_object_pointer_info(state, header.tempClassId, header.tempObjectId)){
                    header = {};
                    return header;
                }
                break;
            default:
                header = {};
                return header;
        }

        if(header.signTypeCode == TypeCode::ID::BeginPointer && header.metaBytes.size() >= sizeof(std::uint32_t)){
            header.tag = read_little_endian<std::uint32_t>(as_bytes(header.metaBytes));
            header.hasTag = true;
        }

        header.hasItem = true;
        return header;
    }
};

[[nodiscard]] std::unique_ptr<IArchiveEngine> make_binary_engine() {
    return std::make_unique<BinaryArchiveEngine>();
}
}  // namespace

BinaryArchive::BinaryArchive(std::shared_ptr<ICountable> counter, std::shared_ptr<IArchiveHook> hook)
    : m_archive(make_binary_engine(), std::move(counter), std::move(hook)) {}

Archive& BinaryArchive::archive() noexcept {
    return m_archive;
}

const Archive& BinaryArchive::archive() const noexcept {
    return m_archive;
}

void BinaryArchive::reset(std::vector<std::uint8_t> buffer, std::uint32_t offset, bool finalized) {
    m_archive.reset_input(std::move(buffer), offset, finalized);
}

ItemHeader BinaryArchive::next() {
    return m_archive.next();
}

bool BinaryArchive::get(TypeCode::ID hopeCode, std::size_t hopeReadSize) {
    return m_archive.get(hopeCode, hopeReadSize);
}

bool BinaryArchive::gets(std::string& str, std::size_t maxSize) {
    return m_archive.gets(str, maxSize);
}

Archive& BinaryArchive::begin() {
    return m_archive.begin();
}

bool BinaryArchive::end() {
    return m_archive.end();
}

bool BinaryArchive::doGet(TypeCode::ID hopeCode) {
    return m_archive.doGet(hopeCode);
}

bool BinaryArchive::getPointer() {
    return m_archive.getPointer();
}

bool BinaryArchive::getObject() {
    return m_archive.getObject();
}

void BinaryArchive::getClassAndModuleList() {
    m_archive.getClassAndModuleList();
}

bool BinaryArchive::check_version() {
    return m_archive.check_version();
}

void BinaryArchive::read_first_section() {
    auto header = m_archive.next();
    auto& state = m_archive.state();
    if(header.signTypeCode == TypeCode::ID::BeginObject && header.tempClassId == 0x04D69F50u){
        state.parsing.itemHeader.hasItem = false;
    }

    if(!get(TypeCode::ID::Int32, 1)){
        return;
    }

    const auto saved_offset = state.input.offset;
    const auto section_offset = read_little_endian<std::uint32_t>(as_bytes(state.parsing.itemHeader.payloadBytes));
    state.input.offset = section_offset;
    state.parsing.itemHeader = {};
    getClassAndModuleList();
    state.input.offset = saved_offset;
    state.parsing.itemHeader = {};
    getPointer();
}
