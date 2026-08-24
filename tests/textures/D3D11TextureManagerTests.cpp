#include "textures/D3D11TextureManager.h"

#include <DirectXTex.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

Microsoft::WRL::ComPtr<ID3D11Device> createDevice() {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL featureLevel{};
    const HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, device.GetAddressOf(), &featureLevel, nullptr);
    if (FAILED(result)) {
        throw std::runtime_error("could not create WARP D3D11 device");
    }
    return device;
}

std::vector<std::uint8_t> makeDds(std::size_t width, std::size_t height) {
    DirectX::ScratchImage source;
    if (FAILED(source.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1))) {
        throw std::runtime_error("could not create DDS fixture");
    }

    DirectX::Blob dds;
    if (FAILED(DirectX::SaveToDDSMemory(
            source.GetImages(), source.GetImageCount(), source.GetMetadata(),
            DirectX::DDS_FLAGS_NONE, dds))) {
        throw std::runtime_error("could not encode DDS fixture");
    }

    const auto* begin = static_cast<const std::uint8_t*>(dds.GetBufferPointer());
    return {begin, begin + dds.GetBufferSize()};
}

void cachesEquivalentPathsWithoutUploadingAgain() {
    const auto device = createDevice();
    stui::textures::D3D11TextureManager manager(device.Get(), 2);
    const auto dds = makeDds(2, 3);
    const std::vector<std::uint8_t> malformed{1, 2, 3};

    const auto first = manager.getOrLoad("pack/tattoo.dds", dds);
    const auto second = manager.getOrLoad("PACK\\TATTOO.DDS", malformed);

    expect(first.has_value() && second.has_value(), "expected cached GPU textures");
    expect(*first == *second, "expected equivalent paths to share one texture");
    expect((*first)->shaderResourceView != nullptr, "expected shader resource view");
    expect((*first)->width == 2 && (*first)->height == 3, "expected DDS dimensions");
}

void evictsAndReleasesLeastRecentlyUsedTexture() {
    const auto device = createDevice();
    stui::textures::D3D11TextureManager manager(device.Get(), 1);
    const auto dds = makeDds(2, 3);

    auto first = manager.getOrLoad("first.dds", dds);
    expect(first.has_value(), "expected first texture upload");
    std::weak_ptr<stui::textures::D3D11Texture> firstTexture = *first;
    first->reset();

    const auto second = manager.getOrLoad("second.dds", dds);

    expect(second.has_value(), "expected second texture upload");
    expect(firstTexture.expired(), "expected eviction to release the first texture");
}

void rejectsUnsafePathBeforeUploading() {
    const auto device = createDevice();
    stui::textures::D3D11TextureManager manager(device.Get(), 2);
    const std::vector<std::uint8_t> malformed{1, 2, 3};

    const auto result = manager.getOrLoad("../tattoo.dds", malformed);

    expect(!result, "expected unsafe path rejection");
    expect(result.error() == stui::textures::TextureManagerError::invalidPath,
           "expected invalidPath before DDS upload");
}

void mapsD3D11UploadFailureToManagerError() {
    const auto device = createDevice();
    stui::textures::D3D11TextureManager manager(device.Get(), 2);
    const std::vector<std::uint8_t> malformed{1, 2, 3};

    const auto result = manager.getOrLoad("broken.dds", malformed);

    expect(!result, "expected malformed DDS failure");
    expect(result.error() == stui::textures::TextureManagerError::uploadFailed,
           "expected uploadFailed manager error");
}

void run(std::string_view name, void (*test)()) {
    try {
        test();
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        throw;
    }
}

}  // namespace

int main() {
    try {
        run("equivalent paths reuse one GPU texture", cachesEquivalentPathsWithoutUploadingAgain);
        run("LRU eviction releases GPU texture", evictsAndReleasesLeastRecentlyUsedTexture);
        run("unsafe paths are rejected before upload", rejectsUnsafePathBeforeUploading);
        run("D3D11 failures map to manager errors", mapsD3D11UploadFailureToManagerError);
    } catch (...) {
        return 1;
    }
    return 0;
}
