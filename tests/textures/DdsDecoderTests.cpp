#include "textures/DdsDecoder.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool decodesValidDdsMetadata() {
    DirectX::ScratchImage source;
    if (FAILED(source.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, 2, 3, 1, 1))) {
        std::cerr << "FAIL could not create DDS fixture\n";
        return false;
    }

    DirectX::Blob dds;
    if (FAILED(DirectX::SaveToDDSMemory(
            source.GetImages(), source.GetImageCount(), source.GetMetadata(),
            DirectX::DDS_FLAGS_NONE, dds))) {
        std::cerr << "FAIL could not encode DDS fixture\n";
        return false;
    }

    const auto bytes = std::span(
        static_cast<const std::uint8_t*>(dds.GetBufferPointer()), dds.GetBufferSize());
    const auto decoded = stui::textures::decodeDds(bytes);
    if (!decoded || decoded->GetMetadata().width != 2 || decoded->GetMetadata().height != 3) {
        std::cerr << "FAIL valid DDS should preserve 2x3 metadata\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> invalidDds{0x44, 0x44, 0x53, 0x20};
    const auto decoded = stui::textures::decodeDds(invalidDds);
    if (decoded || decoded.error() != stui::textures::DdsDecodeError::invalidData) {
        std::cerr << "FAIL broken DDS should return invalidData\n";
        return 1;
    }
    std::cout << "PASS broken DDS returns invalidData\n";
    if (!decodesValidDdsMetadata()) {
        return 1;
    }
    std::cout << "PASS valid DDS preserves metadata\n";
    return 0;
}
