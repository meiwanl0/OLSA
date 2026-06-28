#pragma once

#include <cstdint>

namespace OLSA::Container {
#pragma pack(push, 1)
struct Header {
    char sign[8];
    std::int32_t version;
    char create_info[0x28];
    char padding[0x18];
    std::int16_t node_count;
    std::int16_t dirEntry_node_index;
    std::int16_t valid_node_count;
    std::int16_t code;
    std::int32_t max_size;
};

struct Node {
    std::int32_t offset;
    std::int32_t total_size;
    std::int16_t code;
    std::int16_t data_count;
    std::int16_t data_size;
    std::int16_t index;
};

struct DirEntry {
    char module[0x16];
    char unknown[8];
    std::int16_t code_1;
    std::int16_t code_2;
    std::int16_t code_3;
};
#pragma pack(pop)
}  // namespace OLSA::Container

