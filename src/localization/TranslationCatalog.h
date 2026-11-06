#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace stui::localization {
enum class Language { english, chinese, korean, japanese };
[[nodiscard]] std::string_view languageCode(Language language) noexcept;
[[nodiscard]] const char* languageName(Language language) noexcept;
[[nodiscard]] std::optional<Language> parseLanguage(std::string_view code) noexcept;

// Load once before rendering. Dictionaries are immutable afterwards, so returned
// text pointers survive language changes. Reloading while rendering is unsupported.
class TranslationCatalog {
public:
    [[nodiscard]] bool load(const std::filesystem::path& directory);
    [[nodiscard]] const char* text(Language language, const char* source) const noexcept;
    [[nodiscard]] std::string widget(Language language, std::string_view source) const;
private:
    std::array<std::map<std::string, std::string, std::less<>>, 4> m_dictionaries;
};
[[nodiscard]] TranslationCatalog& catalog() noexcept;
void selectLanguage(Language language) noexcept;
[[nodiscard]] Language activeLanguage() noexcept;
[[nodiscard]] const char* tr(const char* source) noexcept;
[[nodiscard]] std::string widgetLabel(std::string_view source);
}  // namespace stui::localization
