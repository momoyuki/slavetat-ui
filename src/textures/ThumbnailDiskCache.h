#pragma once

#include "textures/TextureResolver.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace stui::textures {

class ThumbnailDiskCache {
public:
    ThumbnailDiskCache(std::filesystem::path directory,
        std::filesystem::path looseRoot,
        std::uintmax_t maxBytes = 256 * 1024 * 1024);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> load(
        std::string_view texturePath) const;
    void store(std::string_view texturePath, std::span<const std::uint8_t> ddsBytes);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> load(const TextureResource& resource) const;
    void store(const TextureResource& resource, std::span<const std::uint8_t> ddsBytes);
    void store(const TextureResource& resource, std::span<const std::uint8_t> ddsBytes, std::uint64_t revision);
    [[nodiscard]] std::optional<std::uint64_t> revision(const TextureResource& resource) const;
    [[nodiscard]] bool clear();
    [[nodiscard]] bool setMaxBytes(std::uintmax_t maxBytes);
    [[nodiscard]] std::uintmax_t bytes() const noexcept { return m_estimatedBytes; }

private:
    [[nodiscard]] std::filesystem::path entryPath(std::string_view texturePath) const;
    [[nodiscard]] std::filesystem::path entryPath(const TextureResource& resource) const;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> loadPath(const std::filesystem::path& path) const;
    void storePath(const std::filesystem::path& path, std::span<const std::uint8_t> ddsBytes);
    [[nodiscard]] bool prune(std::uintmax_t maxBytes);

    std::filesystem::path m_directory;
    std::filesystem::path m_looseRoot;
    std::uint64_t m_archiveStamp{};
    std::uintmax_t m_maxBytes;
    std::uintmax_t m_estimatedBytes{};
};

}  // namespace stui::textures
