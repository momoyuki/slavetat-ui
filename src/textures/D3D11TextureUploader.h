#pragma once

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace stui::textures {

enum class D3D11TextureError {
    invalidData,
    missingDevice,
    uploadFailed,
};

struct D3D11Texture {
    std::shared_ptr<ID3D11ShaderResourceView> shaderResourceView;
    std::size_t width{};
    std::size_t height{};
};

using D3D11TextureResult = std::expected<D3D11Texture, D3D11TextureError>;

[[nodiscard]] D3D11TextureResult uploadDdsTexture(
    ID3D11Device* device,
    std::span<const std::uint8_t> ddsBytes);

}  // namespace stui::textures
