#pragma once

#include "core/TattooModels.h"

#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace stui::runtime {

using IniIntegerReader = std::function<int(
    std::wstring_view section,
    std::wstring_view key,
    int fallback)>;

class OverlaySlotConfiguration {
public:
    OverlaySlotConfiguration();
    explicit OverlaySlotConfiguration(IniIntegerReader reader);

    [[nodiscard]] int count(core::TattooArea area);

private:
    IniIntegerReader m_reader;
    std::array<std::optional<int>, 4> m_counts;
};

}  // namespace stui::runtime
