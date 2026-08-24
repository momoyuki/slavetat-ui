#pragma once

#include "textures/D3D11TextureUploader.h"
#include "textures/TextureManager.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace stui::textures {

using D3D11ManagedTextureResult =
    std::expected<std::shared_ptr<D3D11Texture>, TextureManagerError>;

class D3D11TextureManager {
public:
    D3D11TextureManager(ID3D11Device* device, std::size_t capacity);

    D3D11TextureManager(const D3D11TextureManager&) = delete;
    D3D11TextureManager& operator=(const D3D11TextureManager&) = delete;
    D3D11TextureManager(D3D11TextureManager&&) = delete;
    D3D11TextureManager& operator=(D3D11TextureManager&&) = delete;

    [[nodiscard]] D3D11ManagedTextureResult getOrLoad(
        std::string_view texturePath,
        std::span<const std::uint8_t> ddsBytes);

private:
    std::shared_ptr<ID3D11Device> m_device;
    TextureManager<D3D11Texture> m_textures;
};

}  // namespace stui::textures
