#include "native/NativeSlotWorkflowRuntime.h"

#include <expected>
#include <utility>

namespace stui::native {
namespace {

core::ServiceError operationError(core::ServiceErrorCode code, const char* message) {
    return core::ServiceError{.code = code, .message = message};
}

class InFlightGuard {
public:
    explicit InFlightGuard(std::atomic_bool& inFlight) noexcept : m_inFlight(inFlight) {}
    ~InFlightGuard() {
        m_inFlight.store(false);
    }

    InFlightGuard(const InFlightGuard&) = delete;
    InFlightGuard& operator=(const InFlightGuard&) = delete;

private:
    std::atomic_bool& m_inFlight;
};

}  // namespace

NativeSlotWorkflowRuntime::NativeSlotWorkflowRuntime(
    NativeSlotWorkflowModel& model,
    SlotQueryOperation query,
    SlotApplyOperation apply,
    SlotRemoveOperation remove,
    NativeSlotScheduler scheduler)
    : m_model(model),
      m_query(std::move(query)),
      m_apply(std::move(apply)),
      m_remove(std::move(remove)),
      m_scheduler(std::move(scheduler)) {}

void NativeSlotWorkflowRuntime::pump() {
    bool expected = false;
    if (!m_inFlight.compare_exchange_strong(expected, true)) {
        return;
    }

    if (auto query = m_model.takeSlotQuery()) {
        scheduleQuery(std::move(*query));
        return;
    }
    if (auto apply = m_model.takeApplyRequest()) {
        scheduleApply(std::move(*apply));
        return;
    }
    if (auto remove = m_model.takeRemoveRequest()) {
        scheduleRemove(std::move(*remove));
        return;
    }

    m_inFlight.store(false);
}

void NativeSlotWorkflowRuntime::scheduleQuery(SlotQueryTicket ticket) {
    NativeSlotTask task = [this, ticket] {
        InFlightGuard guard(m_inFlight);
        core::TattooSlotsResult result = std::unexpected(operationError(
            core::ServiceErrorCode::slotQueryFailed,
            "Slot query failed."));
        try {
            result = m_query(ticket.actorFormId, ticket.area);
        } catch (...) {
        }
        m_model.completeSlotQuery(ticket.generation, std::move(result));
    };

    try {
        m_scheduler(std::move(task));
    } catch (...) {
        m_model.completeSlotQuery(
            ticket.generation,
            std::unexpected(operationError(
                core::ServiceErrorCode::slotQueryFailed,
                "Failed to schedule slot query.")));
        m_inFlight.store(false);
    }
}

void NativeSlotWorkflowRuntime::scheduleApply(SlotApplyTicket ticket) {
    const std::uint64_t generation = ticket.generation;
    NativeSlotTask task = [this, ticket = std::move(ticket)] {
        InFlightGuard guard(m_inFlight);
        core::ApplyTattooResult result = std::unexpected(operationError(
            core::ServiceErrorCode::applyFailed,
            "Tattoo apply failed."));
        try {
            result = m_apply(ticket.request);
        } catch (...) {
        }
        m_model.completeApply(ticket.generation, std::move(result));
    };

    try {
        m_scheduler(std::move(task));
    } catch (...) {
        m_model.completeApply(
            generation,
            std::unexpected(operationError(
                core::ServiceErrorCode::applyFailed,
                "Failed to schedule tattoo apply.")));
        m_inFlight.store(false);
    }
}

void NativeSlotWorkflowRuntime::scheduleRemove(SlotRemoveTicket ticket) {
    const std::uint64_t generation = ticket.generation;
    NativeSlotTask task = [this, ticket = std::move(ticket)] {
        InFlightGuard guard(m_inFlight);
        core::RemoveTattooResult result = std::unexpected(operationError(
            core::ServiceErrorCode::removeFailed,
            "Tattoo remove failed."));
        try {
            result = m_remove(ticket.request);
        } catch (...) {
        }
        m_model.completeRemove(ticket.generation, std::move(result));
    };

    try {
        m_scheduler(std::move(task));
    } catch (...) {
        m_model.completeRemove(
            generation,
            std::unexpected(operationError(
                core::ServiceErrorCode::removeFailed,
                "Failed to schedule tattoo remove.")));
        m_inFlight.store(false);
    }
}

}  // namespace stui::native
