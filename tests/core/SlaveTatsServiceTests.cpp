#include "core/ITattooRuntime.h"
#include "core/SlaveTatsService.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using stui::core::ITattooRuntime;
using stui::core::ServiceError;
using stui::core::ServiceErrorCode;
using stui::core::SlaveTatsService;
using stui::core::TattooEntry;
using stui::core::TattooQueryResult;

class FakeTattooRuntime final : public ITattooRuntime {
public:
    [[nodiscard]] bool apiAvailable() const noexcept override {
        return apiAvailableValue;
    }

    [[nodiscard]] bool jContainersReady() const noexcept override {
        return jContainersReadyValue;
    }

    TattooQueryResult queryAvailable(std::string_view domain) override {
        requestedDomain = std::string(domain);
        ++queryCount;
        return queryResult;
    }

    bool apiAvailableValue{true};
    bool jContainersReadyValue{true};
    int queryCount{0};
    std::string requestedDomain;
    TattooQueryResult queryResult{std::vector<TattooEntry>{}};
};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void expectError(const TattooQueryResult& result, ServiceErrorCode code, std::string_view message) {
    expect(!result.has_value(), "expected query to fail");
    expect(result.error().code == code, "unexpected service error code");
    expect(result.error().message == message, "unexpected service error message");
}

void unavailableApiStopsBeforeRuntimeQuery() {
    FakeTattooRuntime runtime;
    runtime.apiAvailableValue = false;
    SlaveTatsService service(runtime);

    const auto result = service.queryAvailable("default");

    expectError(result, ServiceErrorCode::slaveTatsUnavailable, "SlaveTatsNG not available");
    expect(runtime.queryCount == 0, "runtime query must not run without the SlaveTats API");
}

void unavailableJContainersStopsBeforeRuntimeQuery() {
    FakeTattooRuntime runtime;
    runtime.jContainersReadyValue = false;
    SlaveTatsService service(runtime);

    const auto result = service.queryAvailable("default");

    expectError(result, ServiceErrorCode::jContainersUnavailable, "JContainers not ready");
    expect(runtime.queryCount == 0, "runtime query must not run before JContainers is ready");
}

void successfulQueryReturnsCopiedEntriesAndPreservesDomain() {
    FakeTattooRuntime runtime;
    runtime.queryResult = std::vector<TattooEntry>{TattooEntry{
        .runtimeHandle = 42,
        .domain = "custom",
        .section = "Roses",
        .name = "Red Rose",
        .texturePath = "roses/red.dds",
        .area = "Body",
        .slot = -1,
        .color = 0xFFFFFF,
        .locked = false,
        .alpha = 1.0F,
    }};
    SlaveTatsService service(runtime);

    const auto result = service.queryAvailable("custom");

    expect(result.has_value(), "expected query to succeed");
    expect(runtime.requestedDomain == "custom", "service must preserve the requested domain");
    expect(runtime.queryCount == 1, "service must issue exactly one runtime query");
    expect(result.value() == runtime.queryResult.value(), "service must return the runtime entries unchanged");
}

void runtimeFailureIsReturnedUnchanged() {
    FakeTattooRuntime runtime;
    runtime.queryResult = std::unexpected(ServiceError{
        ServiceErrorCode::queryAvailableFailed,
        "query_available_tattoos failed",
    });
    SlaveTatsService service(runtime);

    const auto result = service.queryAvailable("default");

    expectError(result, ServiceErrorCode::queryAvailableFailed, "query_available_tattoos failed");
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
    failures += run("unavailable API stops before runtime query", unavailableApiStopsBeforeRuntimeQuery);
    failures += run("unavailable JContainers stops before runtime query", unavailableJContainersStopsBeforeRuntimeQuery);
    failures += run("successful query returns entries and preserves domain", successfulQueryReturnsCopiedEntriesAndPreservesDomain);
    failures += run("runtime failure is returned unchanged", runtimeFailureIsReturnedUnchanged);
    return failures == 0 ? 0 : 1;
}
