#include "runtime/ThumbnailCacheSetting.h"
#include "runtime/PluginConfigFile.h"

#include <utility>
#include <nlohmann/json.hpp>

namespace stui::runtime {
namespace {
constexpr auto kKey = "thumbnailDiskCacheLimitMiB";
}

ThumbnailCacheSetting::ThumbnailCacheSetting(std::shared_ptr<PluginConfigFile> config)
    : m_config(std::move(config)) {}

bool ThumbnailCacheSetting::load() {
    m_limitMiB.store(defaultLimitMiB);
    if (!m_config) return false;
    const auto config = m_config->read();
    if (!config) return false;
    if (!config->contains(kKey)) return true;
    const auto& value = config->at(kKey);
    if (!value.is_number_integer()) return false;
    // Compare before conversion: large JSON integers must never wrap to a valid choice.
    for (const auto limit : limitsMiB) {
        if (value == limit) {
            m_limitMiB.store(limit);
            return true;
        }
    }
    return false;
}

bool ThumbnailCacheSetting::select(std::uint32_t limitMiB) {
    if (!m_config || !valid(limitMiB)) return false;
    const auto saved = m_config->update([limitMiB](nlohmann::json& config) -> ConfigUpdateResult {
        config[kKey] = limitMiB;
        return {};
    });
    if (!saved) return false;
    m_limitMiB.store(limitMiB);
    return true;
}

std::uint32_t ThumbnailCacheSetting::limitMiB() const noexcept {
    return m_limitMiB.load();
}

}  // namespace stui::runtime
