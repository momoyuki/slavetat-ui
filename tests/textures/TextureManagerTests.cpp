#include "textures/TextureManager.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
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

void reusesUploadForEquivalentTexturePath() {
    stui::textures::TextureManager<int> manager(2);
    const std::vector<std::uint8_t> ddsBytes{1, 2, 3};
    int uploadCount = 0;
    const auto upload = [&](std::span<const std::uint8_t> bytes) {
        ++uploadCount;
        expect(bytes.size() == 3, "expected DDS bytes to reach uploader");
        return std::make_shared<int>(42);
    };

    const auto first = manager.getOrLoad("pack/tattoo.dds", ddsBytes, upload);
    const auto second = manager.getOrLoad("PACK\\TATTOO.DDS", ddsBytes, upload);

    expect(first.has_value() && second.has_value(), "expected uploaded textures");
    expect(*first == *second, "expected equivalent path to reuse GPU resource");
    expect(uploadCount == 1, "expected one GPU upload");
}

void rejectsUnsafePathBeforeUpload() {
    stui::textures::TextureManager<int> manager(2);
    const std::vector<std::uint8_t> ddsBytes{1};
    int uploadCount = 0;
    const auto result = manager.getOrLoad(
        "..\\outside.dds", ddsBytes, [&](std::span<const std::uint8_t>) {
            ++uploadCount;
            return std::make_shared<int>(1);
        });

    expect(!result, "expected unsafe path rejection");
    expect(result.error() == stui::textures::TextureManagerError::invalidPath,
        "expected invalid path error");
    expect(uploadCount == 0, "expected unsafe path to skip upload");
}

void retriesFailedUpload() {
    stui::textures::TextureManager<int> manager(2);
    const std::vector<std::uint8_t> ddsBytes{1};
    int uploadCount = 0;
    const auto fail = [&](std::span<const std::uint8_t>) -> std::shared_ptr<int> {
        ++uploadCount;
        return nullptr;
    };

    const auto first = manager.getOrLoad("broken.dds", ddsBytes, fail);
    const auto second = manager.getOrLoad("broken.dds", ddsBytes, fail);

    expect(!first && !second, "expected failed upload errors");
    expect(first.error() == stui::textures::TextureManagerError::uploadFailed,
        "expected upload failure error");
    expect(uploadCount == 2, "expected failed upload retry");
}

}  // namespace

int main() {
    try {
        reusesUploadForEquivalentTexturePath();
        rejectsUnsafePathBeforeUpload();
        retriesFailedUpload();
        std::cout << "PASS TextureManager contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL equivalent texture path reuses upload: " << error.what() << '\n';
        return 1;
    }
}
