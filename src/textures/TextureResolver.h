#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace stui::textures {

enum class TextureResolveError {
    invalidPath,
    notFound,
    readFailed,
};

enum class TextureSource {
    loose,
    archive,
};

using TextureBytesResult =
    std::expected<std::vector<std::uint8_t>, TextureResolveError>;
using TextureArchiveReader =
    std::function<TextureBytesResult(std::string_view resourcePath)>;

struct ResolvedTexture {
    std::string normalizedPath;
    std::vector<std::uint8_t> bytes;
    TextureSource source{};
};

using TextureResolveResult =
    std::expected<ResolvedTexture, TextureResolveError>;

class TextureResolver {
public:
    explicit TextureResolver(std::filesystem::path looseRoot);

    [[nodiscard]] static std::expected<std::string, TextureResolveError> normalize(
        std::string_view texturePath);
    [[nodiscard]] TextureResolveResult resolve(
        std::string_view texturePath,
        const TextureArchiveReader& archiveReader) const;
    [[nodiscard]] TextureResolveResult resolveLoose(std::string_view texturePath) const;
    [[nodiscard]] TextureResolveResult resolveArchive(
        std::string_view texturePath,
        const TextureArchiveReader& archiveReader) const;

private:
    std::filesystem::path m_looseRoot;
};

}  // namespace stui::textures
