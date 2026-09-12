#include "runtime/UpdateTattooAppearanceOrchestration.h"

#include "runtime/AppliedTattooHandleMembership.h"
#include "runtime/SlaveTatsAlpha.h"

#include <expected>

namespace stui::runtime {

core::UpdateTattooAppearanceResult updateTattooAppearance(
    const core::UpdateTattooAppearanceRequest& request,
    IUpdateTattooAppearanceBackend& backend) {
    const auto actor = backend.resolveActor(request.actorFormId);
    if (!actor) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::actorNotFound,
            "Actor not found",
        });
    }

    if (request.mode == core::UpdateTattooAppearanceMode::updateAndSynchronize) {
        const auto appliedHandles = backend.queryAppliedTattooHandles(actor);
        if (!appliedHandles) {
            return std::unexpected(appliedHandles.error());
        }

        if (!containsAppliedTattooHandle(*appliedHandles, request.runtimeHandle)) {
            return std::unexpected(core::ServiceError{
                core::ServiceErrorCode::staleTattooHandle,
                "Tattoo handle is stale; refresh the slot snapshot and try again",
            });
        }

        if (!backend.writeAppearance(
                request.runtimeHandle,
                request.color,
                toSlaveTatsInvertedAlpha(request.alpha))) {
            return std::unexpected(core::ServiceError{
                core::ServiceErrorCode::updateFailed,
                "Failed to update tattoo appearance",
            });
        }
    }

    if (!backend.markActorUpdated(actor)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::updateFailed,
            "Failed to mark actor tattoos updated",
        });
    }
    if (!backend.synchronize(actor)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::synchronizeFailed,
            request.mode == core::UpdateTattooAppearanceMode::updateAndSynchronize
                ? "Tattoo appearance changed but synchronization failed"
                : "Tattoo synchronization failed",
        });
    }

    return core::UpdateTattooAppearanceSuccess{
        .actorFormId = request.actorFormId,
        .runtimeHandle = request.runtimeHandle,
    };
}

}  // namespace stui::runtime
