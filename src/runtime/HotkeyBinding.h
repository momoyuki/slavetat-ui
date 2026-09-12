#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stui::runtime {

struct HotkeyOption {
    std::string_view label;
    std::optional<std::uint32_t> key;
};

[[nodiscard]] std::span<const HotkeyOption> hotkeyOptions() noexcept;

class HotkeyBinding {
public:
    explicit HotkeyBinding(std::filesystem::path configPath);

    [[nodiscard]] bool load();
    [[nodiscard]] bool select(std::optional<std::uint32_t> key);
    [[nodiscard]] bool matches(std::uint32_t key) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> key() const noexcept;
    [[nodiscard]] std::string label() const;

private:
    [[nodiscard]] bool save() const;

    std::filesystem::path configPath_;
    mutable std::mutex mutex_;
    std::optional<std::uint32_t> key_;
};

}  // namespace stui::runtime
