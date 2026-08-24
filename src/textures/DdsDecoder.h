#pragma once

#include <DirectXTex.h>

#include <cstdint>
#include <expected>
#include <span>

namespace stui::textures {

enum class DdsDecodeError {
    invalidData,
};

using DdsDecodeResult = std::expected<DirectX::ScratchImage, DdsDecodeError>;

[[nodiscard]] DdsDecodeResult decodeDds(std::span<const std::uint8_t> bytes);

}  // namespace stui::textures
