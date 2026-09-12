#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace stui::runtime {

class HotkeyBinding {
public:
    explicit HotkeyBinding(std::filesystem::path configPath);

    [[nodiscard]] bool load();
    void beginCapture() noexcept;
    void cancelCapture() noexcept;
    [[nodiscard]] bool isCapturing() const noexcept;
    [[nodiscard]] bool capture(std::uint32_t key);
    [[nodiscard]] bool clear();
    [[nodiscard]] bool matches(std::uint32_t key) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> key() const noexcept;
    [[nodiscard]] std::string label() const;

private:
    [[nodiscard]] bool save() const;

    std::filesystem::path configPath_;
    mutable std::mutex mutex_;
    std::optional<std::uint32_t> key_;
    bool capturing_{};
};

}  // namespace stui::runtime
