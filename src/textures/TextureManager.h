#pragma once

#include "textures/TextureCache.h"
#include "textures/TextureResolver.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace stui::textures {

enum class TextureManagerError {
    invalidPath,
    uploadFailed,
};

template <class Resource>
class TextureManager {
public:
    explicit TextureManager(std::size_t capacity) : m_cache(capacity) {}

    template <class Uploader>
    std::expected<std::shared_ptr<Resource>, TextureManagerError> getOrLoad(
        std::string_view texturePath,
        std::span<const std::uint8_t> ddsBytes,
        Uploader&& uploader) {
        auto normalized = TextureResolver::normalize(texturePath);
        if (!normalized) {
            return std::unexpected(TextureManagerError::invalidPath);
        }
        for (char& character : *normalized) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }

        auto resource = m_cache.getOrLoad(*normalized, [&]() {
            return std::invoke(std::forward<Uploader>(uploader), ddsBytes);
        });
        if (!resource) {
            return std::unexpected(TextureManagerError::uploadFailed);
        }
        return resource;
    }

private:
    TextureCache<Resource> m_cache;
};

}  // namespace stui::textures
