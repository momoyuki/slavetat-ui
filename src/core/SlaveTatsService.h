#pragma once

#include "core/ITattooRuntime.h"

#include <string_view>

namespace stui::core {

class SlaveTatsService {
public:
    explicit SlaveTatsService(ITattooRuntime& runtime) noexcept;

    TattooQueryResult queryAvailable(std::string_view domain);

private:
    ITattooRuntime& m_runtime;
};

}  // namespace stui::core
