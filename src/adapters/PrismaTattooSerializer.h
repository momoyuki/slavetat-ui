#pragma once

#include "core/TattooModels.h"

#include <string>

namespace stui::adapters {

[[nodiscard]] std::string toPrismaTattooJSON(const core::TattooEntry& tattoo);

}  // namespace stui::adapters
