#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace stui::runtime {

[[nodiscard]] inline bool containsAppliedTattooHandle(
    std::span<const std::int32_t> appliedHandles,
    std::int32_t candidate) noexcept {
    return candidate != 0 &&
        std::ranges::find(appliedHandles, candidate) != appliedHandles.end();
}

}  // namespace stui::runtime
