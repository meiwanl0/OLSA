#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class Compress {
public:
    bool unpack(std::span<std::uint8_t> buffer, std::size_t offset, std::size_t length);
    bool unpack(std::span<const std::uint8_t> source, std::vector<std::uint8_t>& out);
};

