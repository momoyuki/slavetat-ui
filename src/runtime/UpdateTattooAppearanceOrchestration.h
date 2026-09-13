#pragma once

#include "core/TattooModels.h"

#include <cstdint>
#include <expected>
#include <vector>

namespace stui::runtime {

class IUpdateTattooAppearanceBackend {
public:
    using ActorHandle = void*;

    virtual ~IUpdateTattooAppearanceBackend() = default;

    [[nodiscard]] virtual ActorHandle resolveActor(std::uint32_t actorFormId) = 0;
    [[nodiscard]] virtual std::expected<std::vector<std::int32_t>, core::ServiceError>
        queryAppliedTattooHandles(ActorHandle actor) = 0;
    [[nodiscard]] virtual bool writeAppearance(
        std::int32_t runtimeHandle,
        const core::UpdateTattooAppearanceRequest& request) = 0;
    [[nodiscard]] virtual bool markActorUpdated(ActorHandle actor) = 0;
    [[nodiscard]] virtual bool synchronize(ActorHandle actor) = 0;
};

[[nodiscard]] core::UpdateTattooAppearanceResult updateTattooAppearance(
    const core::UpdateTattooAppearanceRequest& request,
    IUpdateTattooAppearanceBackend& backend);

}  // namespace stui::runtime
