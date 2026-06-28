#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace FTML {
template <class T>
struct FlagValue {
    bool has{};
    T value{};
};

struct NameString {
    std::array<std::byte, 0x50> storage{};
};

namespace DC {
struct RawDataSet {
    std::string sampleID;
    void* sysInfo{};
};
}  // namespace DC
}  // namespace FTML

