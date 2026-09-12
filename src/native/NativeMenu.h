#pragma once

#include "native/MenuFrameworkPort.h"

#include <functional>
#include <optional>

namespace stui::native {

class NativeMenu {
public:
    using RenderFunction = std::function<void(NativeMenu&)>;
    using LaunchFunction = std::function<bool()>;

    explicit NativeMenu(RenderFunction render = {}, LaunchFunction launch = {});
    ~NativeMenu();

    NativeMenu(const NativeMenu&) = delete;
    NativeMenu& operator=(const NativeMenu&) = delete;

    [[nodiscard]] RegistrationResult registerMenu(MenuFrameworkPort& port);
    void toggle() noexcept;
    void openFromHotkey() noexcept;
    void handleHotkeyInput(bool isDown, bool isPressed) noexcept;
    void open() noexcept;
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isRegistered() const noexcept;
    [[nodiscard]] std::optional<MenuRegistrationError> lastError() const noexcept;

private:
    static void sectionCallback() noexcept;
    static void renderCallback() noexcept;

    MenuFrameworkPort* port_{};
    MenuWindow window_{};
    RenderFunction render_;
    LaunchFunction launch_;
    std::optional<MenuRegistrationError> lastError_;
    bool registered_{};
    bool waitForHotkeyRelease_{};
};

}  // namespace stui::native
