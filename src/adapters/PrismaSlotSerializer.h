#pragma once

#include "core/TattooModels.h"

#include <string>

namespace stui::adapters {

[[nodiscard]] std::string toPrismaSlotsJSON(const core::TattooSlots& slots);
[[nodiscard]] std::string toPrismaApplySuccessJSON(const core::ApplyTattooSuccess& success);

}  // namespace stui::adapters
