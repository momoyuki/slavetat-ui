#pragma once
#include "localization/TranslationCatalog.h"
#include "runtime/PluginConfigFile.h"
#include <memory>

namespace stui::runtime {
class LanguageSetting {
public:
    explicit LanguageSetting(std::shared_ptr<PluginConfigFile> config);
    [[nodiscard]] bool load();
    [[nodiscard]] bool select(localization::Language language);
    [[nodiscard]] localization::Language language() const noexcept { return m_language; }
private:
    std::shared_ptr<PluginConfigFile> m_config;
    localization::Language m_language{localization::Language::english};
};
}  // namespace stui::runtime
