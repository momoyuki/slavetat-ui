#include "runtime/ThumbnailBackgroundSetting.h"
#include "runtime/PluginConfigFile.h"

#include <utility>
#include <nlohmann/json.hpp>

namespace stui::runtime {
namespace {
constexpr auto kKey = "thumbnailBackground";
}

ThumbnailBackgroundSetting::ThumbnailBackgroundSetting(std::shared_ptr<PluginConfigFile> config)
    : m_config(std::move(config)) {}

bool ThumbnailBackgroundSetting::load() {
    const std::scoped_lock lock(m_mutex);
    m_white.store(false);
    if (!m_config) return false;
    const auto config = m_config->read();
    if (!config) return false;
    if (!config->contains(kKey)) return true;
    const auto& value = config->at(kKey);
    if (!value.is_string() || (value != "black" && value != "white")) return false;
    m_white.store(value == "white");
    return true;
}

bool ThumbnailBackgroundSetting::select(bool white) {
    const std::scoped_lock lock(m_mutex);
    if (!m_config) return false;
    const auto saved = m_config->update([white](nlohmann::json& config) -> ConfigUpdateResult {
        config[kKey] = white ? "white" : "black";
        return {};
    });
    if (!saved) return false;
    m_white.store(white);
    return true;
}

bool ThumbnailBackgroundSetting::white() const noexcept {
    return m_white.load();
}
}  // namespace stui::runtime
