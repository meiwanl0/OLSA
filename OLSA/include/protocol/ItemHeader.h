#pragma once

#include <cstdint>
#include <vector>

#include "TypeCode.h"

namespace FTML::BinaryArchive {
struct ItemHeader {
    TypeCode::ID signTypeCode{TypeCode::ID::None};
    std::uint64_t tag{};
    bool hasTag{};
    bool hasItem{};
    TypeCode::ID payloadType{TypeCode::ID::None};
    std::uint32_t tempClassId{};
    std::uint64_t tempObjectId{};
    std::vector<std::uint8_t> metaBytes;
    std::vector<std::uint8_t> payloadBytes;
};
}  // namespace FTML::BinaryArchive

