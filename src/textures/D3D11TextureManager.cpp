#include "textures/D3D11TextureManager.h"

#include <utility>

namespace stui::textures {
namespace {

std::shared_ptr<ID3D11Device> retainDevice(ID3D11Device* device) {
    if (device == nullptr) {
        return {};
    }

    device->AddRef();
    return std::shared_ptr<ID3D11Device>(device, [](ID3D11Device* retainedDevice) {
        retainedDevice->Release();
    });
}

}  // namespace

D3D11TextureManager::D3D11TextureManager(
    ID3D11Device* device,
    std::size_t capacity)
    : m_device(retainDevice(device)), m_textures(capacity) {}

D3D11ManagedTextureResult D3D11TextureManager::getOrLoad(
    std::string_view texturePath,
    std::span<const std::uint8_t> ddsBytes) {
    return m_textures.getOrLoad(
        texturePath,
        ddsBytes,
        [device = m_device.get()](std::span<const std::uint8_t> bytes) {
            auto uploaded = uploadDdsTexture(device, bytes);
            if (!uploaded) {
                return std::shared_ptr<D3D11Texture>{};
            }
            return std::make_shared<D3D11Texture>(std::move(*uploaded));
        });
}

}  // namespace stui::textures
