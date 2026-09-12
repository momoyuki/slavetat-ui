#pragma once

#include <algorithm>

namespace stui::runtime {

[[nodiscard]] constexpr float toSlaveTatsInvertedAlpha(float visibleAlpha) noexcept {
    return 1.0F - std::clamp(visibleAlpha, 0.0F, 1.0F);
}

[[nodiscard]] constexpr float fromSlaveTatsInvertedAlpha(float invertedAlpha) noexcept {
    return 1.0F - std::clamp(invertedAlpha, 0.0F, 1.0F);
}

}  // namespace stui::runtime
