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
    virtual TattooSlotsResult querySlots(std::uint32_t, TattooArea) {
        return std::unexpected(ServiceError{
            ServiceErrorCode::slotQueryFailed,
            "Slot queries are not implemented",
        });
    }

    virtual ApplyTattooResult applyToSlot(const ApplyTattooRequest&) {
        return std::unexpected(ServiceError{
            ServiceErrorCode::applyFailed,
            "Tattoo apply is not implemented",
        });
    }
};

}  // namespace stui::core
