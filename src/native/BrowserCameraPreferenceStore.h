#pragma once
#include "native/BrowserCameraPose.h"
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
namespace stui::runtime { class PluginConfigFile; }
namespace stui::native {
// Globally shared presentation preferences; contains no native world identity.
class BrowserCameraPreferenceStore {
public:
    explicit BrowserCameraPreferenceStore(std::shared_ptr<runtime::PluginConfigFile> config);
    [[nodiscard]] bool load();
    [[nodiscard]] std::optional<BrowserCameraRelativePose> pose(bool right) const;
    [[nodiscard]] std::uint64_t generation() const;
    [[nodiscard]] bool remember(bool right, const BrowserCameraRelativePose& pose,
        std::uint64_t generation);
    [[nodiscard]] bool clear();
private:
    std::shared_ptr<runtime::PluginConfigFile> m_config;
    mutable std::mutex m_mutex;
    std::array<std::optional<BrowserCameraRelativePose>,2> m_sides;
    std::uint64_t m_generation{};
};
}
