#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

template <class C>
concept ByteContainer = requires(const C& c) {
    { c.data() } -> std::convertible_to<const std::uint8_t*>;
    { c.size() } -> std::convertible_to<std::size_t>;
};

template <ByteContainer C>
[[nodiscard]] std::span<const std::uint8_t> as_bytes(const C& c) {
    return {c.data(), static_cast<std::size_t>(c.size())};
}

template <class T>
requires(std::integral<T> || std::floating_point<T>)
[[nodiscard]] T read_little_endian(std::span<const std::uint8_t> bytes) {
    T value{};
    const auto copy_size = std::min(bytes.size(), sizeof(T));
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(&value, bytes.data(), copy_size);
        return value;
    } else {
        std::array<std::uint8_t, sizeof(T)> tmp{};
        std::copy_n(bytes.data(), copy_size, tmp.data());
        std::reverse(tmp.begin(), tmp.end());
        std::memcpy(&value, tmp.data(), sizeof(T));
        return value;
    }
}

