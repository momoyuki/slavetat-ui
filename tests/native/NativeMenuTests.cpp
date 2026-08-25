#include "native/NativeMenu.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class FakeMenuFrameworkPort final : public stui::native::MenuFrameworkPort {
public:
    [[nodiscard]] bool available() const noexcept override { return isAvailable; }
    [[nodiscard]] float version() const noexcept override { return frameworkVersion; }

    [[nodiscard]] stui::native::RegistrationResult setSection(
        std::string_view value) override {
        section = value;
        return {};
    }

    [[nodiscard]] stui::native::RegistrationResult addSectionItem(
        std::string_view value, stui::native::MenuCallback callback) override {
        itemPath = value;
        itemCallback = callback;
        ++itemRegistrations;
        return {};
    }

    [[nodiscard]] std::expected<stui::native::MenuWindow, stui::native::MenuRegistrationError>
    addWindow(stui::native::MenuCallback callback, bool shouldPauseGame) override {
        windowCallback = callback;
        pauseGame = shouldPauseGame;
        ++windowRegistrations;
        if (windowError) {
            return std::unexpected(*windowError);
        }
        return windowToken;
    }

    void setWindowOpen(stui::native::MenuWindow, bool value) noexcept override { open = value; }
    [[nodiscard]] bool isWindowOpen(stui::native::MenuWindow) const noexcept override {
        return open;
    }

    std::string section;
    std::string itemPath;
    stui::native::MenuCallback itemCallback{};
    stui::native::MenuCallback windowCallback{};
    int itemRegistrations{};
    int windowRegistrations{};
    stui::native::MenuWindow windowToken{1};
    std::optional<stui::native::MenuRegistrationError> windowError;
    float frameworkVersion{3.13F};
    bool isAvailable{true};
    bool pauseGame{true};
    bool open{};
};

void registersOneNonPausingTattooBrowser() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(result.has_value(), "expected native menu registration");
    expect(port.section == "SlaveTatsUI", "expected owning section");
    expect(port.itemPath == "Tattoo Browser", "expected browser item");
    expect(port.itemRegistrations == 1, "expected one browser item");
    expect(port.windowRegistrations == 1, "expected one native window");
    expect(!port.pauseGame, "expected non-pausing window");
}

void repeatedRegistrationIsIdempotent() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;

    expect(menu.registerMenu(port).has_value(), "expected initial registration");
    expect(menu.registerMenu(port).has_value(), "expected repeated registration success");
    expect(port.itemRegistrations == 1, "expected no duplicate browser item");
    expect(port.windowRegistrations == 1, "expected no duplicate native window");
}

void sectionCallbackOpensRegisteredWindow() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;

    expect(menu.registerMenu(port).has_value(), "expected registration");
    expect(port.itemCallback != nullptr, "expected section callback");
    port.itemCallback();

    expect(port.open, "expected section callback to open window");
}

void unavailableFrameworkDoesNotRegisterAnything() {
    FakeMenuFrameworkPort port;
    port.isAvailable = false;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(!result, "expected unavailable registration failure");
    expect(result.error() == stui::native::MenuRegistrationError::unavailable,
           "expected unavailable error");
    expect(port.itemRegistrations == 0 && port.windowRegistrations == 0,
           "expected no registration against unavailable framework");
}

void unsupportedMajorVersionDoesNotRegisterAnything() {
    FakeMenuFrameworkPort port;
    port.frameworkVersion = 4.0F;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(!result, "expected unsupported version failure");
    expect(result.error() == stui::native::MenuRegistrationError::unsupportedVersion,
           "expected unsupported version error");
    expect(port.itemRegistrations == 0 && port.windowRegistrations == 0,
           "expected no registration against unsupported framework");
}

void zeroWindowTokenDoesNotPublishBrowserItem() {
    FakeMenuFrameworkPort port;
    port.windowToken = 0;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(!result, "expected zero window token rejection");
    expect(result.error() == stui::native::MenuRegistrationError::windowCreationFailed,
           "expected window creation error");
    expect(port.itemRegistrations == 0, "expected no browser item without a window");
}

void destroyedMenuMakesSectionCallbackANoOp() {
    FakeMenuFrameworkPort port;
    stui::native::MenuCallback callback{};
    {
        stui::native::NativeMenu menu;
        expect(menu.registerMenu(port).has_value(), "expected registration");
        callback = port.itemCallback;
    }
    port.open = false;

    callback();

    expect(!port.open, "expected callback not to access destroyed menu");
}

void windowRegistrationPreservesNamedFrameworkError() {
    FakeMenuFrameworkPort port;
    port.windowError = stui::native::MenuRegistrationError::missingExport;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(!result, "expected window registration failure");
    expect(result.error() == stui::native::MenuRegistrationError::missingExport,
           "expected named framework error to be preserved");
    expect(port.itemRegistrations == 0, "expected no browser item after window failure");
}

void renderExceptionIsContainedAtCallbackBoundary() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu([] { throw std::runtime_error("render failed"); });
    expect(menu.registerMenu(port).has_value(), "expected registration");

    port.windowCallback();

    expect(menu.lastError() == stui::native::MenuRegistrationError::callbackFailed,
           "expected contained callback failure");
}

void openAndCloseUseRegisteredWindowState() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;
    expect(menu.registerMenu(port).has_value(), "expected registration");
    expect(menu.isRegistered(), "expected registered state");

    menu.open();
    expect(menu.isOpen(), "expected open state");
    menu.close();
    expect(!menu.isOpen(), "expected closed state");
}

}  // namespace

int main() {
    try {
        registersOneNonPausingTattooBrowser();
        std::cout << "PASS registers one non-pausing tattoo browser\n";
        repeatedRegistrationIsIdempotent();
        std::cout << "PASS repeated registration is idempotent\n";
        sectionCallbackOpensRegisteredWindow();
        std::cout << "PASS section callback opens registered window\n";
        unavailableFrameworkDoesNotRegisterAnything();
        std::cout << "PASS unavailable framework does not register anything\n";
        unsupportedMajorVersionDoesNotRegisterAnything();
        std::cout << "PASS unsupported major version does not register anything\n";
        zeroWindowTokenDoesNotPublishBrowserItem();
        std::cout << "PASS zero window token does not publish browser item\n";
        destroyedMenuMakesSectionCallbackANoOp();
        std::cout << "PASS destroyed menu makes section callback a no-op\n";
        windowRegistrationPreservesNamedFrameworkError();
        std::cout << "PASS window registration preserves named framework error\n";
        renderExceptionIsContainedAtCallbackBoundary();
        std::cout << "PASS render exception is contained at callback boundary\n";
        openAndCloseUseRegisteredWindowState();
        std::cout << "PASS open and close use registered window state\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
