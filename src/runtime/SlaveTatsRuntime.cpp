#include "runtime/SlaveTatsRuntime.h"

#include "jcontainers_mini.h"
#include "runtime/SlaveTatsAlpha.h"
#include "runtime/UpdateTattooAppearanceOrchestration.h"
#include "SKSE/SKSE.h"

#include <expected>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

namespace stui::runtime {
namespace {

constexpr const char* kQueryAvailablePool = "SlaveTatsUI-queryAvailable";
constexpr const char* kQuerySlotsExternalPool = "SlaveTatsUI-querySlotsExternal";
constexpr const char* kApplyExternalPool = "SlaveTatsUI-applyExternal";
constexpr const char* kApplyAvailablePool = "SlaveTatsUI-applyAvailable";
constexpr const char* kRemoveExternalPool = "SlaveTatsUI-removeExternal";
constexpr const char* kUpdateAppearancePool = "SlaveTatsUI-updateAppearance";

class JContainerPoolGuard {
public:
    explicit JContainerPoolGuard(const char* pool) noexcept : m_pool(pool) {}
    ~JContainerPoolGuard() { jcmini::JValue::cleanPool(m_pool); }

    JContainerPoolGuard(const JContainerPoolGuard&) = delete;
    JContainerPoolGuard& operator=(const JContainerPoolGuard&) = delete;

private:
    const char* m_pool;
};

const char* areaName(core::TattooArea area) noexcept {
    switch (area) {
    case core::TattooArea::body:
        return "BODY";
    case core::TattooArea::face:
        return "FACE";
    case core::TattooArea::hands:
        return "HANDS";
    case core::TattooArea::feet:
        return "FEET";
    }

    return nullptr;
}

std::expected<std::unordered_set<int>, core::ServiceError> queryExternalSlots(
    const slavetats::interface::Addresses& api,
    RE::Actor* actor,
    const char* area,
    const char* pool) {
    const int matches = jcmini::JValue::addToPool(jcmini::JArray::object(), pool);
    const JContainerPoolGuard poolGuard(pool);
    if (api.external_slots(actor, RE::BSFixedString(area), matches)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::slotQueryFailed,
            "external_slots failed",
        });
    }

    std::unordered_set<int> slots;
    const int count = jcmini::JArray::count(matches);
    for (int index = 0; index < count; ++index) {
        slots.insert(jcmini::JArray::getInt(matches, index));
    }
    return slots;
}

class TattooTemplateAppearanceGuard {
public:
    TattooTemplateAppearanceGuard(int tattoo, int color, float alpha) :
        m_tattoo(tattoo),
        m_color(jcmini::JMap::getInt(tattoo, "color", 0)),
        m_invertedAlpha(jcmini::JMap::getFlt(tattoo, "invertedAlpha", 0.0F)) {
        jcmini::JMap::setInt(m_tattoo, "color", color);
        jcmini::JMap::setFlt(
            m_tattoo,
            "invertedAlpha",
            toSlaveTatsInvertedAlpha(alpha));
    }

    ~TattooTemplateAppearanceGuard() {
        jcmini::JMap::setInt(m_tattoo, "color", m_color);
        jcmini::JMap::setFlt(m_tattoo, "invertedAlpha", m_invertedAlpha);
    }

    TattooTemplateAppearanceGuard(const TattooTemplateAppearanceGuard&) = delete;
    TattooTemplateAppearanceGuard& operator=(const TattooTemplateAppearanceGuard&) = delete;

private:
    int m_tattoo;
    int m_color;
    float m_invertedAlpha;
};

class SlaveTatsAppearanceBackend final : public IUpdateTattooAppearanceBackend {
public:
    SlaveTatsAppearanceBackend(
        const slavetats::interface::Addresses* api,
        const SlaveTatsAppearanceBindings* bindings) noexcept :
        m_api(api), m_bindings(bindings) {}

    ActorHandle resolveActor(std::uint32_t actorFormId) override {
        if (m_bindings) {
            return m_bindings->resolveActor ? m_bindings->resolveActor(actorFormId) : nullptr;
        }
        return RE::TESForm::LookupByID<RE::Actor>(actorFormId);
    }

    std::expected<std::vector<std::int32_t>, core::ServiceError>
    queryAppliedTattooHandles(ActorHandle actorHandle) override {
        if (m_bindings) {
            if (!m_bindings->queryAppliedTattooHandles) {
                return std::unexpected(core::ServiceError{
                    core::ServiceErrorCode::updateFailed,
                    "Applied tattoo query binding is unavailable",
                });
            }
            return m_bindings->queryAppliedTattooHandles(actorHandle);
        }
        auto* actor = static_cast<RE::Actor*>(actorHandle);
        const int matches = jcmini::JValue::addToPool(
            jcmini::JArray::object(),
            kUpdateAppearancePool);
        const JContainerPoolGuard poolGuard(kUpdateAppearancePool);
        if (m_api->query_applied_tattoos(
                actor,
                0,
                matches,
                RE::BSFixedString(""),
                -1)) {
            return std::unexpected(core::ServiceError{
                core::ServiceErrorCode::updateFailed,
                "query_applied_tattoos failed",
            });
        }

        std::vector<std::int32_t> appliedHandles;
        const int count = jcmini::JArray::count(matches);
        appliedHandles.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            appliedHandles.push_back(jcmini::JArray::getObj(matches, index));
        }
        return appliedHandles;
    }

    bool writeAppearance(
        std::int32_t runtimeHandle,
        std::int32_t color,
        float invertedAlpha) override {
        if (m_bindings) {
            if (!m_bindings->setTattooInt || !m_bindings->getTattooInt ||
                !m_bindings->setTattooFloat || !m_bindings->getTattooFloat ||
                runtimeHandle == 0) {
                return false;
            }
            m_bindings->setTattooInt(runtimeHandle, "color", color);
            constexpr auto missingInt = std::numeric_limits<std::int32_t>::min();
            if (m_bindings->getTattooInt(runtimeHandle, "color", missingInt) != color) {
                return false;
            }
            m_bindings->setTattooFloat(runtimeHandle, "invertedAlpha", invertedAlpha);
            const float missingFloat = std::numeric_limits<float>::quiet_NaN();
            return m_bindings->getTattooFloat(
                runtimeHandle, "invertedAlpha", missingFloat) == invertedAlpha;
        }
        if (!jcmini::JMap::setIntAndVerify(runtimeHandle, "color", color)) {
            return false;
        }
        return jcmini::JMap::setFltAndVerify(
            runtimeHandle,
            "invertedAlpha",
            invertedAlpha);
    }

    bool markActorUpdated(ActorHandle actorHandle) override {
        if (m_bindings) {
            if (!m_bindings->setActorInt || !m_bindings->getActorInt || !actorHandle) {
                return false;
            }
            m_bindings->setActorInt(actorHandle, ".SlaveTats.updated", 1);
            constexpr auto missing = std::numeric_limits<std::int32_t>::min();
            return m_bindings->getActorInt(
                actorHandle, ".SlaveTats.updated", missing) == 1;
        }
        return jcmini::JFormDB::setIntAndVerify(
            static_cast<RE::Actor*>(actorHandle), ".SlaveTats.updated", 1);
    }

    bool synchronize(ActorHandle actorHandle) override {
        if (m_bindings) {
            return m_bindings->synchronizeTattoos &&
                !m_bindings->synchronizeTattoos(actorHandle, false);
        }
        return !m_api->synchronize_tattoos(static_cast<RE::Actor*>(actorHandle), false);
    }

private:
    const slavetats::interface::Addresses* m_api;
    const SlaveTatsAppearanceBindings* m_bindings;
};

}  // namespace

void SlaveTatsRuntime::bindSlaveTats(const slavetats::interface::Addresses* api) noexcept {
    m_api = api;
    m_apiVersion = api ? api->current_version : 0;
}

void SlaveTatsRuntime::noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept {
    m_api = nullptr;
    m_apiVersion = version;
}

bool SlaveTatsRuntime::bindJContainers(const jc::root_interface* root) {
    m_jContainersReady = root && jcmini::Init(root);
    return m_jContainersReady;
}

bool SlaveTatsRuntime::apiAvailable() const noexcept {
    return m_api != nullptr;
}

bool SlaveTatsRuntime::jContainersReady() const noexcept {
    return m_jContainersReady;
}

std::uint32_t SlaveTatsRuntime::apiVersion() const noexcept {
    return m_apiVersion;
}

const slavetats::interface::Addresses* SlaveTatsRuntime::api() const noexcept {
    return m_api;
}

core::TattooQueryResult SlaveTatsRuntime::queryAvailable(std::string_view domain) {
    const int matches = jcmini::JValue::addToPool(jcmini::JArray::object(), kQueryAvailablePool);
    const JContainerPoolGuard poolGuard(kQueryAvailablePool);
    const std::string domainString(domain);

    if (m_api->query_available_tattoos(
            0, matches, 0, RE::BSFixedString(domainString.c_str()))) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::queryAvailableFailed,
            "query_available_tattoos failed",
        });
    }

    std::vector<core::TattooEntry> entries;
    const int count = jcmini::JArray::count(matches);
    entries.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const int handle = jcmini::JArray::getObj(matches, index);
        const int rawColor = jcmini::JMap::getInt(handle, "color", 0);
        entries.push_back(core::TattooEntry{
            .runtimeHandle = handle,
            .domain = domainString,
            .section = jcmini::JMap::getStr(handle, "section"),
            .name = jcmini::JMap::getStr(handle, "name"),
            .texturePath = jcmini::JMap::getStr(handle, "texture"),
            .area = jcmini::JMap::getStr(handle, "area"),
            .slot = jcmini::JMap::getInt(handle, "slot"),
            .color = rawColor == 0 ? 0xFFFFFF : rawColor,
            .locked = jcmini::JMap::getInt(handle, "locked") != 0,
            .alpha = fromSlaveTatsInvertedAlpha(
                jcmini::JMap::getFlt(handle, "invertedAlpha", 0.0F)),
        });
    }

    return entries;
}

core::TattooSlotsResult SlaveTatsRuntime::querySlots(
    std::uint32_t actorFormId,
    core::TattooArea area) {
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
    if (!actor) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::actorNotFound,
            "Actor not found",
        });
    }

    const char* areaString = areaName(area);
    if (!areaString) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::invalidArea,
            "Invalid tattoo area",
        });
    }

    const auto externalSlots = queryExternalSlots(
        *m_api,
        actor,
        areaString,
        kQuerySlotsExternalPool);
    if (!externalSlots) {
        return std::unexpected(externalSlots.error());
    }

    const int configuredCount = m_slotConfiguration.count(area);
    core::TattooSlots result{
        .actorFormId = actorFormId,
        .area = area,
        .configuredCount = configuredCount,
    };
    if (configuredCount > 0) {
        result.slots.reserve(static_cast<std::size_t>(configuredCount));
    }

    for (int slot = 0; slot < configuredCount; ++slot) {
        const int tattoo = m_api->get_applied_tattoo_in_slot(
            actor,
            RE::BSFixedString(areaString),
            slot);
        if (tattoo != 0) {
            const int rawColor = jcmini::JMap::getInt(tattoo, "color", 0);
            result.slots.push_back(core::TattooSlot{
                .index = slot,
                .occupancy = core::SlotOccupancy::slaveTats,
                .tattoo = core::TattooEntry{
                    .runtimeHandle = tattoo,
                    .domain = jcmini::JMap::getStr(tattoo, "domain", "default"),
                    .section = jcmini::JMap::getStr(tattoo, "section"),
                    .name = jcmini::JMap::getStr(tattoo, "name"),
                    .texturePath = jcmini::JMap::getStr(tattoo, "texture"),
                    .area = areaString,
                    .slot = slot,
                    .color = rawColor == 0 ? 0xFFFFFF : rawColor,
                    .locked = jcmini::JMap::getInt(tattoo, "locked") != 0,
                    .alpha = fromSlaveTatsInvertedAlpha(
                        jcmini::JMap::getFlt(tattoo, "invertedAlpha", 0.0F)),
                },
            });
        } else if (externalSlots->contains(slot)) {
            result.slots.push_back(core::TattooSlot{
                .index = slot,
                .occupancy = core::SlotOccupancy::external,
            });
        } else {
            result.slots.push_back(core::TattooSlot{
                .index = slot,
                .occupancy = core::SlotOccupancy::empty,
            });
        }
    }

    return result;
}

core::ApplyTattooResult SlaveTatsRuntime::applyToSlot(const core::ApplyTattooRequest& request) {
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(request.actorFormId);
    if (!actor) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::actorNotFound,
            "Actor not found",
        });
    }

    const char* areaString = areaName(request.area);
    if (!areaString) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::invalidArea,
            "Invalid tattoo area",
        });
    }

    const auto externalSlots = queryExternalSlots(
        *m_api,
        actor,
        areaString,
        kApplyExternalPool);
    if (!externalSlots) {
        return std::unexpected(externalSlots.error());
    }
    if (externalSlots->contains(request.slot)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::externalSlot,
            "Slot is occupied by an external overlay",
        });
    }

    const int available = jcmini::JValue::addToPool(
        jcmini::JArray::object(),
        kApplyAvailablePool);
    const JContainerPoolGuard poolGuard(kApplyAvailablePool);
    if (m_api->query_available_tattoos(
            0,
            available,
            0,
            RE::BSFixedString(request.domain.c_str()))) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::applyFailed,
            "query_available_tattoos failed",
        });
    }

    int tattooHandle = 0;
    const int count = jcmini::JArray::count(available);
    for (int index = 0; index < count; ++index) {
        const int candidate = jcmini::JArray::getObj(available, index);
        if (jcmini::JMap::getStr(candidate, "section") == request.section &&
            jcmini::JMap::getStr(candidate, "name") == request.name) {
            tattooHandle = candidate;
            break;
        }
    }

    if (tattooHandle == 0) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::tattooNotFound,
            "Tattoo not found in available list",
        });
    }

    int applied = 0;
    {
        const TattooTemplateAppearanceGuard appearance(
            tattooHandle,
            request.color,
            request.alpha);
        applied = m_api->add_and_get_tattoo(
            actor,
            tattooHandle,
            request.slot,
            false,
            false,
            true);
    }

    if (applied == 0) {
        SKSE::log::warn(
            "SlaveTatsUI: SlaveTatsNG rejected tattoo apply (section={}, name={}, area={}, slot={})",
            request.section,
            request.name,
            areaString,
            request.slot);
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::applyFailed,
            "Failed to apply tattoo to slot",
        });
    }

    jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
    if (m_api->synchronize_tattoos(actor, false)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::synchronizeFailed,
            "Tattoo applied but synchronization failed",
        });
    }

    return core::ApplyTattooSuccess{
        .actorFormId = request.actorFormId,
        .area = request.area,
        .slot = request.slot,
        .section = request.section,
        .name = request.name,
    };
}

core::RemoveTattooResult SlaveTatsRuntime::removeFromSlot(
    const core::RemoveTattooRequest& request) {
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(request.actorFormId);
    if (!actor) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::actorNotFound,
            "Actor not found",
        });
    }

    const char* areaString = areaName(request.area);
    if (!areaString) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::invalidArea,
            "Invalid tattoo area",
        });
    }

    if (request.mode == core::RemoveTattooMode::removeAndSynchronize) {
        const auto externalSlots = queryExternalSlots(
            *m_api,
            actor,
            areaString,
            kRemoveExternalPool);
        if (!externalSlots) {
            return std::unexpected(externalSlots.error());
        }
        if (externalSlots->contains(request.slot)) {
            return std::unexpected(core::ServiceError{
                core::ServiceErrorCode::externalSlot,
                "Slot is occupied by an external overlay",
            });
        }

        if (m_api->remove_tattoo_from_slot(
                actor,
                RE::BSFixedString(areaString),
                request.slot,
                false,
                false)) {
            return std::unexpected(core::ServiceError{
                core::ServiceErrorCode::removeFailed,
                "Failed to remove tattoo from slot",
            });
        }
    }

    jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
    if (m_api->synchronize_tattoos(actor, false)) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::synchronizeFailed,
            "Tattoo removed but synchronization failed",
        });
    }

    return core::RemoveTattooSuccess{
        .actorFormId = request.actorFormId,
        .area = request.area,
        .slot = request.slot,
    };
}

core::UpdateTattooAppearanceResult SlaveTatsRuntime::updateAppearance(
    const core::UpdateTattooAppearanceRequest& request) {
    SlaveTatsAppearanceBackend backend(
        m_api,
        m_appearanceBindings ? &*m_appearanceBindings : nullptr);
    return updateTattooAppearance(request, backend);
}

}  // namespace stui::runtime
