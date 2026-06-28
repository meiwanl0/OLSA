#include "../include/archive/Compress.h"

#include <algorithm>
#include <cstring>

namespace {
struct PackedLayoutHeader {
    std::uint32_t count{};
    std::uint16_t element_size{};
    std::uint16_t layout_mode{};
};

[[nodiscard]] bool parse_layout_header(std::span<const std::uint8_t> src, PackedLayoutHeader& out) {
    if(src.size() < 8){
        return false;
    }

    std::memcpy(&out.count, src.data(), sizeof(out.count));
    std::memcpy(&out.element_size, src.data() + sizeof(out.count), sizeof(out.element_size));
    std::memcpy(&out.layout_mode, src.data() + sizeof(out.count) + sizeof(out.element_size), sizeof(out.layout_mode));
    return out.count != 0 && out.element_size != 0;
}

[[nodiscard]] bool reorder_plane_major_bytes(
    const PackedLayoutHeader& layout,
    std::span<const std::uint8_t> plane_major,
    std::vector<std::uint8_t>& out) {
    const auto total = static_cast<std::size_t>(layout.count) * layout.element_size;
    if(plane_major.size() != total){
        return false;
    }

    out.resize(total);
    if(layout.element_size == 1){
        std::memcpy(out.data(), plane_major.data(), total);
        return true;
    }

    for(std::uint32_t element_index = 0; element_index < layout.count; ++element_index){
        for(std::uint16_t byte_index = 0; byte_index < layout.element_size; ++byte_index){
            out[static_cast<std::size_t>(element_index) * layout.element_size + byte_index] =
                plane_major[static_cast<std::size_t>(byte_index) * layout.count + element_index];
        }
    }
    return true;
}

[[nodiscard]] bool unpack_impl(std::span<const std::uint8_t> src, std::vector<std::uint8_t>& out) {
    PackedLayoutHeader layout{};
    if(!parse_layout_header(src, layout)){
        return false;
    }

    const auto decompressed_size = static_cast<std::size_t>(layout.count) * layout.element_size;
    std::size_t src_ptr = 8;
    std::vector<std::uint8_t> plane_major;
    plane_major.reserve(decompressed_size);

    while(src_ptr < src.size() && plane_major.size() < decompressed_size){
        const std::uint8_t byte = src[src_ptr++];

        if((byte & 0xC0) == 0x80){
            std::uint32_t count = 0;
            if((byte & 0xE0) == 0x80){
                count = (byte & 0x1F) + 1;
            }else if((byte & 0xE0) == 0xA0){
                if(src_ptr >= src.size()){
                    return false;
                }
                count = (((byte & 0x1F) << 8) | src[src_ptr++]) + 1;
            }else{
                return false;
            }

            if(src_ptr + count > src.size()){
                return false;
            }
            const auto copy_size = std::min<std::size_t>(count, decompressed_size - plane_major.size());
            plane_major.insert(plane_major.end(), src.begin() + src_ptr, src.begin() + src_ptr + copy_size);
            src_ptr += count;
            continue;
        }

        if((byte & 0xC0) == 0xC0){
            const auto mode = static_cast<std::uint8_t>(byte & 0xE0);
            std::uint32_t count = 0;
            auto prev_byte = static_cast<std::uint8_t>((byte & 0xFE) << 3);
            if(mode == 0xE0){
                if(src_ptr >= src.size()){
                    return false;
                }
                count = (((byte & 1) << 8) | src[src_ptr++]) + 2;
            }else if(mode == 0xC0){
                count = (byte & 1) + 2;
            }else{
                return false;
            }

            for(std::uint32_t i = 0; i < count && src_ptr < src.size(); ++i){
                const auto nibbles = src[src_ptr++];
                const auto hi = static_cast<std::uint8_t>(nibbles >> 4);
                const auto lo = static_cast<std::uint8_t>(nibbles & 0x0F);

                prev_byte = static_cast<std::uint8_t>(prev_byte + hi - 7);
                if(plane_major.size() < decompressed_size){
                    plane_major.push_back(prev_byte);
                }

                prev_byte = static_cast<std::uint8_t>(prev_byte + lo - 7);
                if(plane_major.size() < decompressed_size){
                    plane_major.push_back(prev_byte);
                }
            }
            continue;
        }

        const auto bucket = static_cast<std::uint8_t>(byte & 0xE0);
        const auto wide_bucket = static_cast<std::uint8_t>(byte & 0xF8);

        if(bucket == 0x20){
            if(src_ptr + 2 > src.size()){
                return false;
            }
            const auto count = static_cast<std::uint32_t>(((byte & 0x1F) << 8) | src[src_ptr++]) + 1;
            const auto value = src[src_ptr++];
            for(std::uint32_t i = 0; i < count && plane_major.size() < decompressed_size; ++i){
                plane_major.push_back(value);
            }
            continue;
        }

        if((wide_bucket & 0xF0) == 0x50){
            if(src_ptr + 3 > src.size()){
                return false;
            }
            const auto count = static_cast<std::uint32_t>(((byte & 0x0F) << 8) | src[src_ptr++]) + 1;
            auto value = src[src_ptr++];
            const auto step = src[src_ptr++];
            for(std::uint32_t i = 0; i < count && plane_major.size() < decompressed_size; ++i){
                plane_major.push_back(value);
                value = static_cast<std::uint8_t>(value + step);
            }
            continue;
        }

        if(wide_bucket >= 0x60 && wide_bucket <= 0x78){
            if(src_ptr >= src.size()){
                return false;
            }
            auto value = src[src_ptr++];
            std::uint8_t step = 0;
            if(wide_bucket == 0x60) step = 0xFE;
            if(wide_bucket == 0x68) step = 0xFF;
            if(wide_bucket == 0x70) step = 0x01;
            if(wide_bucket == 0x78) step = 0x02;
            const auto count = static_cast<std::uint32_t>(byte & 0x07) + 1;
            for(std::uint32_t i = 0; i < count && plane_major.size() < decompressed_size; ++i){
                plane_major.push_back(value);
                value = static_cast<std::uint8_t>(value + step);
            }
            continue;
        }

        if(bucket == 0x40){
            if(src_ptr + 2 > src.size()){
                return false;
            }
            auto value = src[src_ptr++];
            const auto step = src[src_ptr++];
            const auto count = static_cast<std::uint32_t>(byte & 0x0F) + 1;
            for(std::uint32_t i = 0; i < count && plane_major.size() < decompressed_size; ++i){
                plane_major.push_back(value);
                value = static_cast<std::uint8_t>(value + step);
            }
            continue;
        }

        if(bucket == 0x00){
            if(src_ptr >= src.size()){
                return false;
            }
            const auto value = src[src_ptr++];
            const auto count = static_cast<std::uint32_t>(byte & 0x1F) + 1;
            for(std::uint32_t i = 0; i < count && plane_major.size() < decompressed_size; ++i){
                plane_major.push_back(value);
            }
            continue;
        }

        return false;
    }

    return plane_major.size() == decompressed_size
        && reorder_plane_major_bytes(layout, std::span<const std::uint8_t>(plane_major.data(), plane_major.size()), out);
}
}  // namespace

bool Compress::unpack(std::span<std::uint8_t> buffer, std::size_t offset, std::size_t length) {
    if(offset > buffer.size() || offset + length > buffer.size()){
        return false;
    }
    std::vector<std::uint8_t> out;
    if(!unpack_impl(std::span<const std::uint8_t>(buffer.data() + offset, length), out)){
        return false;
    }
    if(offset + out.size() > buffer.size()){
        return false;
    }
    std::memcpy(buffer.data() + offset, out.data(), out.size());
    return true;
}

bool Compress::unpack(std::span<const std::uint8_t> source, std::vector<std::uint8_t>& out) {
    return unpack_impl(source, out);
}
