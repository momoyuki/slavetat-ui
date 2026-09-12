#include "textures/D3D11TextureUploader.h"

#include "textures/DdsDecoder.h"

#include <DirectXTex.h>

namespace stui::textures {

D3D11TextureResult uploadDdsTexture(
    ID3D11Device* device,
    std::span<const std::uint8_t> ddsBytes) {
    if (device == nullptr) {
        return std::unexpected(D3D11TextureError::missingDevice);
    }

    auto decoded = decodeDds(ddsBytes);
    if (!decoded) {
        return std::unexpected(D3D11TextureError::invalidData);
    }

    D3D11Texture texture;
    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    const auto& metadata = decoded->GetMetadata();
    const HRESULT result = DirectX::CreateShaderResourceView(
        device,
        decoded->GetImages(),
        decoded->GetImageCount(),
        metadata,
        &shaderResourceView);
    if (FAILED(result)) {
        return std::unexpected(D3D11TextureError::uploadFailed);
    }
    texture.shaderResourceView = std::shared_ptr<ID3D11ShaderResourceView>(
        shaderResourceView, [](ID3D11ShaderResourceView* view) {
            view->Release();
        });

    texture.width = metadata.width;
    texture.height = metadata.height;
    return texture;
}

}  // namespace stui::textures
