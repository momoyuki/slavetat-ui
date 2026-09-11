#pragma once

#include "core/TattooModels.h"

#include <string>

namespace stui::adapters {

[[nodiscard]] std::string toPrismaTattooJSON(const core::TattooEntry& tattoo);
[[nodiscard]] std::string toPrismaUpdateTattooSuccessJSON();

}  // namespace stui::adapters
