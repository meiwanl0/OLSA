#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace FTML::BinaryArchive::TypeCode {
enum class ID : std::uint8_t {
    None = 0,
    Bool = 1,
    Char = 2,
    SChar = 3,
    Int8 = 4,
    Int16 = 5,
    Int32 = 6,
    Int64 = 7,
    UChar = 8,
    UInt8 = 9,
    UInt16 = 10,
    UInt32 = 11,
    UInt64 = 12,
    Float = 13,
    Double = 14,
    LDouble = 15,
    FloatComplex = 16,
    DoubleComplex = 17,
    LongComplex = 18,
    BeginGroup = 0x21,
    EndGroup = 0x22,
    BeginObject = 0x23,
    EndObject = 0x24,
    BeginPointer = 0x25,
    EndPointer = 0x26
};

struct PrimitiveTraits {
    const char* name;
    std::uint32_t size;
    bool is_integer;
    bool is_signed;
    bool is_float;
};

inline constexpr std::array<PrimitiveTraits, 19> kPrimitiveTraits{{
    {"None", 0, false, false, false},
    {"bool", 1, false, false, false},
    {"char", 1, true, true, false},
    {"schar", 1, true, true, false},
    {"int8", 1, true, true, false},
    {"int16", 2, true, true, false},
    {"int32", 4, true, true, false},
    {"int64", 8, true, true, false},
    {"uchar", 1, true, false, false},
    {"uint8", 1, true, false, false},
    {"uint16", 2, true, false, false},
    {"uint32", 4, true, false, false},
    {"uint64", 8, true, false, false},
    {"float", 4, false, true, true},
    {"double", 8, false, true, true},
    {"ldouble", 10, false, true, true},
    {"FloatComplex", 8, false, true, true},
    {"DoubleComplex", 16, false, true, true},
    {"LongComplex", 20, false, true, true},
}};

inline constexpr std::uint32_t size(std::uint8_t type_index) noexcept {
    return type_index < kPrimitiveTraits.size() ? kPrimitiveTraits[type_index].size : 0u;
}

inline constexpr bool is_integer(std::uint8_t type_index) noexcept {
    return type_index < kPrimitiveTraits.size() && kPrimitiveTraits[type_index].is_integer;
}

inline constexpr bool is_signed(std::uint8_t type_index) noexcept {
    return type_index < kPrimitiveTraits.size() && kPrimitiveTraits[type_index].is_signed;
}

inline constexpr bool is_float(std::uint8_t type_index) noexcept {
    return type_index < kPrimitiveTraits.size() && kPrimitiveTraits[type_index].is_float;
}

inline std::string name(std::uint8_t type_index) {
    if(type_index < kPrimitiveTraits.size()){
        return kPrimitiveTraits[type_index].name;
    }
    switch(static_cast<ID>(type_index)){
        case ID::BeginGroup:
            return "BeginGroup";
        case ID::EndGroup:
            return "EndGroup";
        case ID::BeginObject:
            return "BeginObject";
        case ID::EndObject:
            return "EndObject";
        case ID::BeginPointer:
            return "BeginPointer";
        case ID::EndPointer:
            return "EndPointer";
        default:
            return "_Unknown_";
    }
}

template <class T>
inline T read_scalar(const void* src, std::size_t src_size) {
    T value{};
    std::memcpy(&value, src, std::min(src_size, sizeof(T)));
    return value;
}

template <class T>
inline void write_scalar(void* dst, std::size_t dst_size, T value) {
    std::memcpy(dst, &value, std::min(dst_size, sizeof(T)));
}

inline bool convert(ID dst_type, void* dst, ID src_type, const void* src, std::size_t count) {
    if(dst == nullptr || src == nullptr){
        return false;
    }
    if(count == 0){
        return true;
    }

    const auto dst_size = size(static_cast<std::uint8_t>(dst_type));
    const auto src_size = size(static_cast<std::uint8_t>(src_type));
    if(dst_size == 0 || src_size == 0){
        return false;
    }

    if(dst_type == src_type){
        std::memcpy(dst, src, dst_size * count);
        return true;
    }

    const auto* src_bytes = static_cast<const std::uint8_t*>(src);
    auto* dst_bytes = static_cast<std::uint8_t*>(dst);
    const auto src_float = is_float(static_cast<std::uint8_t>(src_type));
    const auto dst_float = is_float(static_cast<std::uint8_t>(dst_type));
    const auto src_int = is_integer(static_cast<std::uint8_t>(src_type)) || src_type == ID::Bool;
    const auto dst_int = is_integer(static_cast<std::uint8_t>(dst_type));

    if(dst_type == ID::Bool){
        for(std::size_t i = 0; i < count; ++i){
            bool any = false;
            for(std::size_t j = 0; j < src_size; ++j){
                any = any || (src_bytes[i * src_size + j] != 0);
            }
            dst_bytes[i] = any ? 1u : 0u;
        }
        return true;
    }

    if(!((src_int || src_float) && (dst_int || dst_float))){
        return false;
    }

    for(std::size_t i = 0; i < count; ++i){
        const void* in = src_bytes + i * src_size;
        void* out = dst_bytes + i * dst_size;
        const double numeric = src_float
            ? (src_type == ID::Float
                   ? static_cast<double>(read_scalar<float>(in, src_size))
                   : read_scalar<double>(in, src_size))
            : (is_signed(static_cast<std::uint8_t>(src_type))
                   ? static_cast<double>(read_scalar<std::int64_t>(in, src_size))
                   : static_cast<double>(read_scalar<std::uint64_t>(in, src_size)));

        if(dst_float){
            if(dst_type == ID::Float){
                write_scalar<float>(out, dst_size, static_cast<float>(numeric));
            }else{
                write_scalar<double>(out, dst_size, numeric);
            }
            continue;
        }

        if(is_signed(static_cast<std::uint8_t>(dst_type))){
            write_scalar<std::int64_t>(out, dst_size, static_cast<std::int64_t>(numeric));
        }else{
            write_scalar<std::uint64_t>(out, dst_size, static_cast<std::uint64_t>(numeric));
        }
    }

    return true;
}
}  // namespace FTML::BinaryArchive::TypeCode

