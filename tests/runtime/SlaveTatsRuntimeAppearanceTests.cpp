#include "runtime/SlaveTatsRuntime.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using stui::core::ServiceErrorCode;
using stui::core::UpdateTattooAppearanceMode;
using stui::core::UpdateTattooAppearanceRequest;
using stui::runtime::SlaveTatsAppearanceBindings;
using stui::runtime::SlaveTatsRuntime;

struct BindingState {
    void* actor{reinterpret_cast<void*>(0x1234)};
    void* queriedActor{};
    void* updatedActor{};
    void* synchronizedActor{};
    bool synchronizationFailed{};
    bool persistUpdated{true};
    int queryCount{};
    int integerWriteCount{};
    int floatWriteCount{};
    int updatedWriteCount{};
    int synchronizeCount{};
    bool synchronizedSilently{true};
    std::vector<std::int32_t> handles{73};
    std::unordered_map<std::int32_t, std::int32_t> integers;
    std::unordered_map<std::int32_t, float> floats;
    std::int32_t updatedValue{};
    std::string integerKey;
    std::string floatKey;
    std::string updatedPath;
};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

SlaveTatsAppearanceBindings bindingsFor(BindingState& state) {
    return SlaveTatsAppearanceBindings{
        .resolveActor = [&state](std::uint32_t actorFormId) -> void* {
            return actorFormId == 0x14 ? state.actor : nullptr;
        },
        .queryAppliedTattooHandles = [&state](void* actor) {
            ++state.queryCount;
            state.queriedActor = actor;
            return stui::runtime::AppliedTattooHandleQueryResult{state.handles};
        },
        .setTattooInt = [&state](std::int32_t handle, const char* key, std::int32_t value) {
            ++state.integerWriteCount;
            state.integerKey = key;
            state.integers[handle] = value;
        },
        .getTattooInt = [&state](std::int32_t handle, const char*, std::int32_t fallback) {
            const auto found = state.integers.find(handle);
            return found == state.integers.end() ? fallback : found->second;
        },
        .setTattooFloat = [&state](std::int32_t handle, const char* key, float value) {
            ++state.floatWriteCount;
            state.floatKey = key;
            state.floats[handle] = value;
        },
        .getTattooFloat = [&state](std::int32_t handle, const char*, float fallback) {
            const auto found = state.floats.find(handle);
            return found == state.floats.end() ? fallback : found->second;
        },
        .setActorInt = [&state](void* actor, const char* path, std::int32_t value) {
            ++state.updatedWriteCount;
            state.updatedActor = actor;
            state.updatedPath = path;
            if (state.persistUpdated) {
                state.updatedValue = value;
            }
        },
        .getActorInt = [&state](void*, const char*, std::int32_t fallback) {
            return state.persistUpdated ? state.updatedValue : fallback;
        },
        .synchronizeTattoos = [&state](void* actor, bool silent) {
            ++state.synchronizeCount;
            state.synchronizedActor = actor;
            state.synchronizedSilently = silent;
            return state.synchronizationFailed;
        },
    };
}

UpdateTattooAppearanceRequest request(UpdateTattooAppearanceMode mode =
        UpdateTattooAppearanceMode::updateAndSynchronize) {
    return UpdateTattooAppearanceRequest{
        .actorFormId = 0x14,
        .runtimeHandle = 73,
        .color = 0x123456,
        .alpha = 0.35F,
        .mode = mode,
    };
}

void productionDelegationQueriesRequestedActorAndWritesExactAppearance() {
    BindingState state;
    SlaveTatsRuntime runtime(bindingsFor(state));

    const auto result = runtime.updateAppearance(request());

    expect(result.has_value(), "expected production runtime update to succeed");
    expect(state.queryCount == 1 && state.queriedActor == state.actor,
        "expected actor-specific applied-tattoo query");
    expect(state.integerWriteCount == 1 && state.integerKey == "color" &&
            state.integers[73] == 0x123456,
        "expected exact RGB write and readback");
    expect(state.floatWriteCount == 1 && state.floatKey == "invertedAlpha" &&
            std::abs(state.floats[73] - 0.65F) < 0.0001F,
        "expected exact inverted-alpha write and readback");
    expect(state.updatedWriteCount == 1 && state.updatedActor == state.actor &&
            state.updatedPath == ".SlaveTats.updated" && state.updatedValue == 1,
        "expected truthful actor updated marker");
    expect(state.synchronizeCount == 1 && state.synchronizedActor == state.actor &&
            !state.synchronizedSilently,
        "expected exactly one synchronize call with SlaveTats silent=false polarity");
}

void staleHandlesNeverWriteMarkOrSynchronize() {
    for (const auto handles : std::vector<std::vector<std::int32_t>>{{}, {74}, {0}}) {
        BindingState state;
        state.handles = handles;
        SlaveTatsRuntime runtime(bindingsFor(state));

        auto staleRequest = request();
        if (handles == std::vector<std::int32_t>{0}) {
            staleRequest.runtimeHandle = 0;
        }
        const auto result = runtime.updateAppearance(staleRequest);

        expect(!result && result.error().code == ServiceErrorCode::staleTattooHandle,
            "expected zero, absent, or foreign handle to be stale");
        expect(state.integerWriteCount == 0 && state.floatWriteCount == 0 &&
                state.updatedWriteCount == 0 && state.synchronizeCount == 0,
            "expected stale path to perform no write, mark, or synchronization");
    }
}

void missingActorStopsBeforeQueryOrMutation() {
    BindingState state;
    state.actor = nullptr;
    SlaveTatsRuntime runtime(bindingsFor(state));

    const auto result = runtime.updateAppearance(request());

    expect(!result && result.error().code == ServiceErrorCode::actorNotFound,
        "expected missing requested actor to return actorNotFound");
    expect(state.queryCount == 0 && state.integerWriteCount == 0 &&
            state.floatWriteCount == 0 && state.updatedWriteCount == 0 &&
            state.synchronizeCount == 0,
        "expected missing actor path not to query, write, mark, or synchronize");
}

void ineffectiveAppearanceReadbacksStopBeforeUpdatedMarkerAndSynchronization() {
    {
        BindingState state;
        auto bindings = bindingsFor(state);
        bindings.getTattooInt = [](std::int32_t, const char*, std::int32_t fallback) {
            return fallback;
        };
        SlaveTatsRuntime runtime(std::move(bindings));

        const auto result = runtime.updateAppearance(request());
        expect(!result && result.error().code == ServiceErrorCode::updateFailed,
            "expected ineffective color write to return updateFailed");
        expect(state.integerWriteCount == 1 && state.floatWriteCount == 0 &&
                state.updatedWriteCount == 0 && state.synchronizeCount == 0,
            "expected color readback failure to stop later writes and synchronization");
    }

    {
        BindingState state;
        auto bindings = bindingsFor(state);
        bindings.getTattooFloat = [](std::int32_t, const char*, float) {
            return 0.650001F;
        };
        SlaveTatsRuntime runtime(std::move(bindings));

        const auto result = runtime.updateAppearance(request());
        expect(!result && result.error().code == ServiceErrorCode::updateFailed,
            "expected non-exact inverted-alpha readback to return updateFailed");
        expect(state.integerWriteCount == 1 && state.floatWriteCount == 1 &&
                state.updatedWriteCount == 0 && state.synchronizeCount == 0,
            "expected alpha readback failure to stop updated marker and synchronization");
    }
}

void failedUpdatedReadbackStopsBeforeSynchronization() {
    BindingState state;
    state.persistUpdated = false;
    SlaveTatsRuntime runtime(bindingsFor(state));

    const auto result = runtime.updateAppearance(request());

    expect(!result && result.error().code == ServiceErrorCode::updateFailed,
        "expected stable updateFailed when updated marker does not stick");
    expect(state.integerWriteCount == 1 && state.floatWriteCount == 1 &&
            state.updatedWriteCount == 1 && state.synchronizeCount == 0,
        "expected updated readback failure to stop before synchronization");
}

void synchronizeOnlyMarksAndUsesFailurePolarityWithoutAppearanceWrites() {
    BindingState state;
    state.synchronizationFailed = true;
    SlaveTatsRuntime runtime(bindingsFor(state));

    const auto result = runtime.updateAppearance(request(UpdateTattooAppearanceMode::synchronizeOnly));

    expect(!result && result.error().code == ServiceErrorCode::synchronizeFailed,
        "expected SlaveTats true return to map to synchronizeFailed");
    expect(state.queryCount == 0 && state.integerWriteCount == 0 &&
            state.floatWriteCount == 0,
        "expected synchronize-only path not to query or write appearance");
    expect(state.updatedWriteCount == 1 && state.synchronizeCount == 1,
        "expected synchronize-only path to mark and synchronize exactly once");
}

template <class Test>
int run(std::string_view name, Test&& test) {
    try {
        std::forward<Test>(test)();
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        return 1;
    }
}

}  // namespace

int main() {
    int failures = 0;
    failures += run("production delegation queries actor and writes exact appearance",
        productionDelegationQueriesRequestedActorAndWritesExactAppearance);
    failures += run("stale handles never mutate or synchronize",
        staleHandlesNeverWriteMarkOrSynchronize);
    failures += run("missing actor stops before query or mutation",
        missingActorStopsBeforeQueryOrMutation);
    failures += run("ineffective appearance readbacks stop before synchronization",
        ineffectiveAppearanceReadbacksStopBeforeUpdatedMarkerAndSynchronization);
    failures += run("failed updated readback stops before synchronization",
        failedUpdatedReadbackStopsBeforeSynchronization);
    failures += run("synchronize-only preserves no-write and failure polarity",
        synchronizeOnlyMarksAndUsesFailurePolarityWithoutAppearanceWrites);
    return failures == 0 ? 0 : 1;
}
