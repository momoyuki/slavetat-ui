#pragma once

#include "core/TattooModels.h"

#include <string_view>

namespace stui::core {

class ITattooRuntime {
public:
    virtual ~ITattooRuntime() = default;

    [[nodiscard]] virtual bool apiAvailable() const noexcept = 0;
    [[nodiscard]] virtual bool jContainersReady() const noexcept = 0;
    virtual TattooQueryResult queryAvailable(std::string_view domain) = 0;
};

}  // namespace stui::core
