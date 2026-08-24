#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace stui::repository {

inline constexpr std::wstring_view kTattooSourceRelativeDirectory =
    L"textures/actors/character/SlaveTats";
inline constexpr std::string_view kTattooSourceIdPrefix =
    "textures/actors/character/slavetats/";

struct TattooSourceFile {
    std::string sourceId;
    std::string sourceFile;
    std::string packName;
    std::filesystem::path effectivePath;
};

using TattooSourceScanResult =
    std::expected<std::vector<TattooSourceFile>, std::error_code>;

[[nodiscard]] TattooSourceScanResult scanTattooSources(
    const std::filesystem::path& directory);

}  // namespace stui::repository
