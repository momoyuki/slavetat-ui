#pragma once

#include <atomic>
#include <memory>
#include <mutex>

namespace stui::runtime {
class PluginConfigFile;

class ThumbnailBackgroundSetting {
public:
    explicit ThumbnailBackgroundSetting(std::shared_ptr<PluginConfigFile> config);
    [[nodiscard]] bool load();
    [[nodiscard]] bool select(bool white);
    [[nodiscard]] bool white() const noexcept;

private:
    std::shared_ptr<PluginConfigFile> m_config;
    std::mutex m_mutex;
    std::atomic_bool m_white{false};
};
}  // namespace stui::runtime
