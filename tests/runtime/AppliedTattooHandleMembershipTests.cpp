#include "runtime/AppliedTattooHandleMembership.h"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void exactAppliedHandleIsAccepted() {
    constexpr std::array appliedHandles{101, 202, 303};

    expect(
        stui::runtime::containsAppliedTattooHandle(appliedHandles, 202),
        "expected exact applied tattoo handle to be accepted");
}

void zeroHandleIsRejected() {
    constexpr std::array appliedHandles{0, 101, 202};

    expect(
        !stui::runtime::containsAppliedTattooHandle(appliedHandles, 0),
        "expected zero tattoo handle to be rejected");
}

void absentHandleIsRejected() {
    constexpr std::array appliedHandles{101, 202, 303};

    expect(
        !stui::runtime::containsAppliedTattooHandle(appliedHandles, 404),
        "expected absent tattoo handle to be rejected");
}

void foreignActorHandleIsRejected() {
    constexpr std::array requestedActorHandles{101, 202};
    constexpr std::array foreignActorHandles{303};

    expect(
        stui::runtime::containsAppliedTattooHandle(foreignActorHandles, 303),
        "expected fixture handle to belong to the foreign actor");
    expect(
        !stui::runtime::containsAppliedTattooHandle(requestedActorHandles, 303),
        "expected another actor's tattoo handle to be rejected");
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
    failures += run("exact applied handle is accepted", exactAppliedHandleIsAccepted);
    failures += run("zero handle is rejected", zeroHandleIsRejected);
    failures += run("absent handle is rejected", absentHandleIsRejected);
    failures += run("foreign actor handle is rejected", foreignActorHandleIsRejected);
    return failures == 0 ? 0 : 1;
}
