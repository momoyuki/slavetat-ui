#include "textures/TextureResolver.h"

#include <fstream>
#include <iterator>
#include <utility>

namespace stui::textures {
namespace {

inline constexpr std::string_view kTattooTextureResourceRoot =
    "textures\\actors\\character\\slavetats\\";

TextureBytesResult readLooseBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::unexpected(TextureResolveError::readFailed);
    }

    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return std::unexpected(TextureResolveError::readFailed);
    }
    return bytes;
}

std::expected<std::filesystem::path, TextureResolveError> containedLoosePath(
    const std::filesystem::path& root,
    std::string_view normalizedPath) {
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return std::unexpected(TextureResolveError::readFailed);
    }

    const auto candidate = std::filesystem::weakly_canonical(
        root / std::filesystem::path(normalizedPath), error);
    if (error) {
        return std::unexpected(TextureResolveError::readFailed);
    }

    const auto relative = std::filesystem::relative(candidate, canonicalRoot, error);
    if (error || relative.is_absolute()) {
        return std::unexpected(TextureResolveError::invalidPath);
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return std::unexpected(TextureResolveError::invalidPath);
        }
    }
    return candidate;
}

TextureResolveResult resolveArchiveBytes(
    std::string normalizedPath,
    const TextureArchiveReader& archiveReader) {
    if (!archiveReader) {
        return std::unexpected(TextureResolveError::readFailed);
    }
    auto bytes = archiveReader(
        std::string(kTattooTextureResourceRoot) + normalizedPath);
    if (!bytes) {
        return std::unexpected(bytes.error());
    }
    return ResolvedTexture{
        .normalizedPath = std::move(normalizedPath),
        .bytes = std::move(*bytes),
        .source = TextureSource::archive,
    };
}

}  // namespace

TextureResolver::TextureResolver(std::filesystem::path looseRoot)
    : m_looseRoot(std::move(looseRoot)) {}

std::expected<std::string, TextureResolveError> TextureResolver::normalize(
    std::string_view texturePath) {
    if (texturePath.empty() || texturePath.front() == '/' || texturePath.front() == '\\' ||
        texturePath.find(':') != std::string_view::npos) {
        return std::unexpected(TextureResolveError::invalidPath);
    }

    std::string normalized;
    std::string segment;
    const auto appendSegment = [&]() -> bool {
        if (segment.empty() || segment == ".") {
            segment.clear();
            return true;
        }
        if (segment == "..") {
            return false;
        }
        if (!normalized.empty()) {
            normalized.push_back('\\');
        }
        normalized += segment;
        segment.clear();
        return true;
    };

    for (const char character : texturePath) {
        if (character == '/' || character == '\\') {
            if (!appendSegment()) {
                return std::unexpected(TextureResolveError::invalidPath);
            }
        } else {
            segment.push_back(character);
        }
    }
    if (!appendSegment() || normalized.empty()) {
        return std::unexpected(TextureResolveError::invalidPath);
    }
    return normalized;
}

TextureResolveResult TextureResolver::resolve(
    std::string_view texturePath,
    const TextureArchiveReader& archiveReader) const {
    auto normalized = normalize(texturePath);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }

    auto loosePath = containedLoosePath(m_looseRoot, *normalized);
    if (!loosePath) {
        return std::unexpected(loosePath.error());
    }
    std::error_code error;
    const bool looseExists = std::filesystem::is_regular_file(*loosePath, error);
    if (error == std::errc::no_such_file_or_directory) {
        error.clear();
    }
    if (error) {
        return std::unexpected(TextureResolveError::readFailed);
    }
    if (looseExists) {
        auto bytes = readLooseBytes(*loosePath);
        if (!bytes) {
            return std::unexpected(bytes.error());
        }
        return ResolvedTexture{
            .normalizedPath = std::move(*normalized),
            .bytes = std::move(*bytes),
            .source = TextureSource::loose,
        };
    }

    return resolveArchiveBytes(std::move(*normalized), archiveReader);
}

TextureResolveResult TextureResolver::resolveLoose(std::string_view texturePath) const {
    return resolve(texturePath, [](std::string_view) {
        return TextureBytesResult(
            std::unexpected(TextureResolveError::notFound));
    });
}

TextureResolveResult TextureResolver::resolveArchive(
    std::string_view texturePath,
    const TextureArchiveReader& archiveReader) const {
    auto normalized = normalize(texturePath);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }

    return resolveArchiveBytes(std::move(*normalized), archiveReader);
}

}  // namespace stui::textures
