#include "native/D3D11NativeThumbnailSource.h"

#include <DirectXTex.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using stui::native::D3D11NativeThumbnailSource;
using stui::native::NativeThumbnailFailure;
using stui::native::NativeThumbnailFailureStage;
using stui::textures::TextureBytesResult;
using stui::textures::TextureResolveError;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() /
            ("slavetats-ui-native-thumbnail-source-" + std::to_string(unique));
        fs::create_directories(m_path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(m_path, error);
    }

    void write(const fs::path& relativePath, const std::vector<std::uint8_t>& bytes) const {
        fs::create_directories((m_path / relativePath).parent_path());
        std::ofstream stream(m_path / relativePath, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    [[nodiscard]] const fs::path& path() const noexcept { return m_path; }

private:
    fs::path m_path;
};

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

std::unique_ptr<D3D11NativeThumbnailSource> makeSource(
    const fs::path& root,
    ID3D11Device* device,
    stui::textures::TextureArchiveReader archiveReader) {
    return std::make_unique<D3D11NativeThumbnailSource>(
        root, device, 12, 2min, std::move(archiveReader));
}

void looseFileWinsWithoutArchiveRead() {
    TemporaryDirectory directory;
    const auto dds = makeDds(2, 3);
    directory.write(fs::path("Pack") / "loose.dds", dds);
    const auto device = createDevice();
    std::size_t archiveReads{};
    auto source = makeSource(directory.path(), device.Get(), [&](std::string_view) {
        ++archiveReads;
        return TextureBytesResult(std::vector<std::uint8_t>{1, 2, 3});
    });

    const auto loaded = source->load("Pack/loose.dds", [] { return true; }, {});

    expect(loaded.has_value(), "expected loose DDS to upload");
    expect(archiveReads == 0, "expected loose DDS to skip archive reader");
}

void looseMissFallsBackToCanonicalArchivePath() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    std::string requestedPath;
    const auto dds = makeDds(2, 3);
    auto source = makeSource(directory.path(), device.Get(), [&](std::string_view path) {
        requestedPath = path;
        return TextureBytesResult(dds);
    });

    const auto loaded = source->load("Pack/archive.dds", [] { return true; }, {});

    expect(loaded.has_value(), "expected archive DDS to upload");
    expect(requestedPath == "textures\\actors\\character\\slavetats\\Pack\\archive.dds",
        "expected canonical archive resource path");
}

void invalidPathMapsToBroken() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    auto source = makeSource(directory.path(), device.Get(), [](std::string_view) {
        return TextureBytesResult(std::unexpected(TextureResolveError::notFound));
    });

    const auto loaded = source->load("../outside.dds", [] { return true; }, {});

    expect(!loaded && loaded.error() == NativeThumbnailFailure::broken,
        "expected invalid path to map to broken");
}

void missingSourcesMapToMissing() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    auto source = makeSource(directory.path(), device.Get(), [](std::string_view) {
        return TextureBytesResult(std::unexpected(TextureResolveError::notFound));
    });

    const auto loaded = source->load("Pack/missing.dds", [] { return true; }, {});

    expect(!loaded && loaded.error() == NativeThumbnailFailure::missing,
        "expected absent loose and archive DDS to map to missing");
}

void malformedDdsMapsToBroken() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    auto source = makeSource(directory.path(), device.Get(), [](std::string_view) {
        return TextureBytesResult(std::vector<std::uint8_t>{1, 2, 3});
    });

    const auto loaded = source->load("Pack/broken.dds", [] { return true; }, {});

    expect(!loaded && loaded.error() == NativeThumbnailFailure::broken,
        "expected malformed DDS to map to broken");
}

void uploadFailureReportsItsStageAndTexturePath() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    std::string reportedPath;
    NativeThumbnailFailureStage reportedStage{};
    auto source = std::make_unique<D3D11NativeThumbnailSource>(
        directory.path(),
        device.Get(),
        12,
        2min,
        [](std::string_view) {
            return TextureBytesResult(std::vector<std::uint8_t>{1, 2, 3});
        },
        [&](std::string_view path, NativeThumbnailFailureStage stage) {
            reportedPath = path;
            reportedStage = stage;
        });

    const auto loaded = source->load("Pack/broken.dds", [] { return true; }, {});

    expect(!loaded && loaded.error() == NativeThumbnailFailure::broken,
        "expected malformed DDS to remain broken");
    expect(reportedPath == "Pack/broken.dds",
        "expected failure report to retain the requested texture path");
    expect(reportedStage == NativeThumbnailFailureStage::upload,
        "expected malformed DDS to report the upload stage");
}

void cancellationAfterResolveAvoidsCacheEntry() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    bool current = true;
    const auto dds = makeDds(2, 3);
    auto source = makeSource(directory.path(), device.Get(), [&](std::string_view) {
        current = false;
        return TextureBytesResult(dds);
    });

    const auto loaded = source->load("Pack/cancelled.dds", [&] { return current; }, {});

    expect(!loaded && loaded.error() == NativeThumbnailFailure::cancelled,
        "expected cancellation after resolve");
    expect(!source->find("Pack/cancelled.dds", {}),
        "expected cancelled load to leave no cache entry");
}

void cacheHitReusesTextureWithoutSecondArchiveRead() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    const auto dds = makeDds(2, 3);
    std::size_t archiveReads{};
    auto source = makeSource(directory.path(), device.Get(), [&](std::string_view) {
        ++archiveReads;
        return TextureBytesResult(dds);
    });

    const auto first = source->load("Pack/cached.dds", [] { return true; }, {});
    const auto second = source->load(
        "Pack/cached.dds", [] { return true; }, stui::textures::TextureCacheTimePoint{} + 1s);

    expect(first.has_value() && second.has_value(), "expected cached loads to succeed");
    expect(*first == *second, "expected cache hit to return the same texture");
    expect(archiveReads == 1, "expected cache hit to skip a second archive read");
}

void clearRemovesCacheEntries() {
    TemporaryDirectory directory;
    const auto device = createDevice();
    const auto dds = makeDds(2, 3);
    auto source = makeSource(directory.path(), device.Get(), [&](std::string_view) {
        return TextureBytesResult(dds);
    });

    const auto loaded = source->load("Pack/clear.dds", [] { return true; }, {});
    expect(loaded.has_value(), "expected cache fixture load");

    source->clear();

    expect(!source->find("Pack/clear.dds", {}), "expected clear to remove cache entry");
}

void nullDeviceConstructsUnavailablePlaceholderSourceWithoutArchiveRead() {
    TemporaryDirectory directory;
    bool archiveRead{};
    auto source = makeSource(directory.path(), nullptr, [&](std::string_view) {
        archiveRead = true;
        return TextureBytesResult(std::vector<std::uint8_t>{1, 2, 3});
    });

    expect(!source->available(), "expected null device source to remain unavailable");
    const auto loaded = source->load("Pack/unavailable.dds", [] { return true; }, {});
    expect(!loaded && loaded.error() == NativeThumbnailFailure::deviceUnavailable,
        "expected unavailable source load failure without a crash");
    source->pruneExpired({});
    source->clear();
    expect(!source->find("Pack/unavailable.dds", {}),
        "expected unavailable source to retain no cached texture");
    expect(!archiveRead, "expected unavailable source to skip archive reads");
}

template <class Test>
int run(std::string_view name, Test&& test) {
    try {
        std::forward<Test>(test)();
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        return 1;
    }
}

}  // namespace

int main() {
    int failures = 0;
    failures += run("loose file wins without archive read", looseFileWinsWithoutArchiveRead);
    failures += run("loose miss falls back to canonical archive path", looseMissFallsBackToCanonicalArchivePath);
    failures += run("invalid path maps to broken", invalidPathMapsToBroken);
    failures += run("missing sources map to missing", missingSourcesMapToMissing);
    failures += run("malformed DDS maps to broken", malformedDdsMapsToBroken);
    failures += run(
        "upload failure reports stage and texture path",
        uploadFailureReportsItsStageAndTexturePath);
    failures += run("cancellation after resolve avoids cache entry", cancellationAfterResolveAvoidsCacheEntry);
    failures += run("cache hit reuses texture without second archive read", cacheHitReusesTextureWithoutSecondArchiveRead);
    failures += run("clear removes cache entries", clearRemovesCacheEntries);
    failures += run(
        "null device constructs unavailable placeholder source without archive read",
        nullDeviceConstructsUnavailablePlaceholderSourceWithoutArchiveRead);
    return failures == 0 ? 0 : 1;
}
