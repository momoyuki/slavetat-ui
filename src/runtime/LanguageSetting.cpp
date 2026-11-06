#include "runtime/LanguageSetting.h"
#include <nlohmann/json.hpp>
#include <utility>

namespace stui::runtime {
LanguageSetting::LanguageSetting(std::shared_ptr<PluginConfigFile> config) : m_config(std::move(config)) {}
bool LanguageSetting::load() {
    m_language = localization::Language::english;
    if (!m_config) return false;
    const auto config = m_config->read();
    if (!config) return false;
    if (!config->contains("language")) return true;
    const auto& value = config->at("language");
    if (!value.is_string()) return false;
    const auto parsed = localization::parseLanguage(value.get<std::string>());
    if (!parsed) return false;
    m_language = *parsed;
    return true;
}
bool LanguageSetting::select(localization::Language language) {
    if (!m_config || static_cast<unsigned int>(language) > static_cast<unsigned int>(localization::Language::japanese)) return false;
    const auto saved = m_config->update([language](nlohmann::json& config) -> ConfigUpdateResult {
        config["language"] = localization::languageCode(language);
        return {};
    });
    if (!saved) return false;
    m_language = language;
    return true;
}
}  // namespace stui::runtime
