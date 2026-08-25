#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace stui::native {

enum class MenuRegistrationError {
    unavailable,
    unsupportedVersion,
    missingExport,
    windowCreationFailed,
    callbackFailed,
};

using MenuCallback = void (*)() noexcept;
using MenuWindow = std::uintptr_t;
using RegistrationResult = std::expected<void, MenuRegistrationError>;

class MenuFrameworkPort {
public:
    virtual ~MenuFrameworkPort() = default;

    [[nodiscard]] virtual bool available() const noexcept = 0;
    [[nodiscard]] virtual float version() const noexcept = 0;
    [[nodiscard]] virtual RegistrationResult setSection(std::string_view section) = 0;
    [[nodiscard]] virtual RegistrationResult addSectionItem(
        std::string_view path, MenuCallback callback) = 0;
    [[nodiscard]] virtual std::expected<MenuWindow, MenuRegistrationError> addWindow(
        MenuCallback callback, bool pauseGame) = 0;
    virtual void setWindowOpen(MenuWindow window, bool open) noexcept = 0;
    [[nodiscard]] virtual bool isWindowOpen(MenuWindow window) const noexcept = 0;
};

}  // namespace stui::native
