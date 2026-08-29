#include "runtime/OverlaySlotConfiguration.h"

#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

namespace stui::runtime {
namespace {

struct AreaSetting {
    std::size_t index;
    std::wstring_view section;
    int fallback;
};

std::optional<AreaSetting> settingFor(core::TattooArea area) noexcept {
    switch (area) {
    case core::TattooArea::body:
        return AreaSetting{0, L"Overlays/Body", 12};
    case core::TattooArea::face:
        return AreaSetting{1, L"Overlays/Face", 3};
    case core::TattooArea::hands:
        return AreaSetting{2, L"Overlays/Hands", 3};
    case core::TattooArea::feet:
        return AreaSetting{3, L"Overlays/Feet", 3};
    }

    return std::nullopt;
}

IniIntegerReader makeProductionReader() {
    wchar_t executablePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    const auto iniPath = std::filesystem::path(executablePath).parent_path()
        / L"Data" / L"SKSE" / L"Plugins" / L"skee64.ini";

    return [iniPath](std::wstring_view section, std::wstring_view key, int fallback) {
        const std::wstring sectionString(section);
        const std::wstring keyString(key);
        return static_cast<int>(GetPrivateProfileIntW(
            sectionString.c_str(),
            keyString.c_str(),
            fallback,
            iniPath.c_str()));
    };
}

}  // namespace

OverlaySlotConfiguration::OverlaySlotConfiguration() :
    OverlaySlotConfiguration(makeProductionReader()) {}

OverlaySlotConfiguration::OverlaySlotConfiguration(IniIntegerReader reader) :
    m_reader(std::move(reader)) {}

int OverlaySlotConfiguration::count(core::TattooArea area) {
    const auto setting = settingFor(area);
    if (!setting) {
        return 0;
    }

    auto& cached = m_counts[setting->index];
    if (!cached) {
        cached = m_reader(setting->section, L"iNumOverlays", setting->fallback);
    }

    return *cached;
}

}  // namespace stui::runtime
