#pragma once

#include <string>
#include <utility>

namespace stui::textures { class TextureResolver; }

namespace stui::core {

enum class TextureResourceKind { catalog, external };
class TextureResource {
public:
    [[nodiscard]] TextureResourceKind kind() const noexcept { return m_kind; }
    [[nodiscard]] const std::string& path() const noexcept { return m_path; }
    bool operator==(const TextureResource&) const = default;
private:
    friend class stui::textures::TextureResolver;
    TextureResource(TextureResourceKind kind, std::string path)
        : m_kind(kind), m_path(std::move(path)) {}
    TextureResourceKind m_kind;
    std::string m_path;
};

}  // namespace stui::core
