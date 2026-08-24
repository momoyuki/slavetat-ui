#pragma once

#include <cstdint>
#include <expected>
#include <vector>

namespace stui::textures {

enum class ExactStreamReadError {
    readFailed,
};

template <class Stream>
[[nodiscard]] std::expected<std::vector<std::uint8_t>, ExactStreamReadError>
readExactBytes(Stream& stream, std::uint32_t size) {
    std::vector<std::uint8_t> bytes(size);
    if (size > 0 && !stream.read(bytes.data(), size)) {
        return std::unexpected(ExactStreamReadError::readFailed);
    }
    return bytes;
}

}  // namespace stui::textures
