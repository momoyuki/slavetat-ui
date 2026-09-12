#include "textures/DdsDecoder.h"

namespace stui::textures {

DdsDecodeResult decodeDds(std::span<const std::uint8_t> bytes) {
    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    const HRESULT result = DirectX::LoadFromDDSMemory(
        bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(result)) {
        return std::unexpected(DdsDecodeError::invalidData);
    }
    return image;
}

}  // namespace stui::textures
