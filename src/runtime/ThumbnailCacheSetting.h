#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace stui::runtime {

class PluginConfigFile;

class ThumbnailCacheSetting {
public:
    static constexpr std::uint32_t defaultLimitMiB = 256;
    static constexpr std::array<std::uint32_t, 4> limitsMiB{128, 256, 512, 1024};
    [[nodiscard]] static constexpr bool valid(std::uint32_t value) noexcept {
        return value == 128 || value == 256 || value == 512 || value == 1024;
    }
    explicit ThumbnailCacheSetting(std::shared_ptr<PluginConfigFile> config);
    [[nodiscard]] bool load();
    [[nodiscard]] bool select(std::uint32_t limitMiB);
    [[nodiscard]] std::uint32_t limitMiB() const noexcept;

private:
    std::shared_ptr<PluginConfigFile> m_config;
    std::atomic_uint32_t m_limitMiB{defaultLimitMiB};
};

}  // namespace stui::runtime
