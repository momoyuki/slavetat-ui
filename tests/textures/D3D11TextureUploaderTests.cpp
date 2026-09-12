#include "textures/D3D11TextureUploader.h"

#include <DirectXTex.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

int main() {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL featureLevel{};
    const HRESULT deviceResult = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, device.GetAddressOf(), &featureLevel, nullptr);
    if (FAILED(deviceResult)) {
        std::cerr << "FAIL could not create WARP D3D11 device\n";
        return 1;
    }

    DirectX::ScratchImage source;
    if (FAILED(source.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 3, 1, 1))) {
        std::cerr << "FAIL could not create DDS fixture\n";
        return 1;
    }
    DirectX::Blob dds;
    if (FAILED(DirectX::SaveToDDSMemory(
            source.GetImages(), source.GetImageCount(), source.GetMetadata(),
            DirectX::DDS_FLAGS_NONE, dds))) {
        std::cerr << "FAIL could not encode DDS fixture\n";
        return 1;
    }

    const auto bytes = std::span(
        static_cast<const std::uint8_t*>(dds.GetBufferPointer()), dds.GetBufferSize());

    const auto missingDevice = stui::textures::uploadDdsTexture(nullptr, bytes);
    if (missingDevice ||
        missingDevice.error() != stui::textures::D3D11TextureError::missingDevice) {
        std::cerr << "FAIL null device should return missingDevice\n";
        return 1;
    }

    const std::vector<std::uint8_t> malformedDds{1, 2, 3};
    const auto invalidData = stui::textures::uploadDdsTexture(device.Get(), malformedDds);
    if (invalidData ||
        invalidData.error() != stui::textures::D3D11TextureError::invalidData) {
        std::cerr << "FAIL malformed DDS should return invalidData\n";
        return 1;
    }

    std::cout << "PASS uploader rejects missing device and malformed DDS\n";

    const auto texture = stui::textures::uploadDdsTexture(device.Get(), bytes);
    if (!texture || !texture->shaderResourceView ||
        texture->width != 2 || texture->height != 3) {
        std::cerr << "FAIL valid DDS should create a 2x3 shader resource view\n";
        return 1;
    }

    std::cout << "PASS valid DDS creates D3D11 shader resource view\n";
    return 0;
}
