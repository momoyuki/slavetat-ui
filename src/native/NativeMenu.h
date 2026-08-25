#pragma once

#include "native/MenuFrameworkPort.h"

#include <functional>
#include <optional>

namespace stui::native {

class NativeMenu {
public:
    explicit NativeMenu(std::function<void()> render = {});
    ~NativeMenu();

    NativeMenu(const NativeMenu&) = delete;
    NativeMenu& operator=(const NativeMenu&) = delete;

    [[nodiscard]] RegistrationResult registerMenu(MenuFrameworkPort& port);
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
    std::function<void()> render_;
    std::optional<MenuRegistrationError> lastError_;
    bool registered_{};
};

}  // namespace stui::native
