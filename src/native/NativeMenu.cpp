#include "native/NativeMenu.h"

#include <utility>

namespace stui::native {
namespace {

NativeMenu* g_activeMenu{};

}  // namespace

NativeMenu::NativeMenu(std::function<void()> render) : render_(std::move(render)) {}

NativeMenu::~NativeMenu() {
    if (g_activeMenu == this) {
        g_activeMenu = nullptr;
    }
}

RegistrationResult NativeMenu::registerMenu(MenuFrameworkPort& port) {
    if (registered_) {
        return {};
    }
    if (!port.available()) {
        return std::unexpected(MenuRegistrationError::unavailable);
    }
    const float frameworkVersion = port.version();
    if (frameworkVersion < 3.0F || frameworkVersion >= 4.0F) {
        return std::unexpected(MenuRegistrationError::unsupportedVersion);
    }
    if (auto result = port.setSection("SlaveTatsUI"); !result) {
        return result;
    }
    auto window = port.addWindow(&NativeMenu::renderCallback, false);
    if (!window) {
        return std::unexpected(window.error());
    }
    if (*window == 0) {
        return std::unexpected(MenuRegistrationError::windowCreationFailed);
    }
    if (auto result = port.addSectionItem("Tattoo Browser", &NativeMenu::sectionCallback);
        !result) {
        return result;
    }

    port_ = &port;
    window_ = *window;
    registered_ = true;
    g_activeMenu = this;
    return {};
}

std::optional<MenuRegistrationError> NativeMenu::lastError() const noexcept {
    return lastError_;
}

void NativeMenu::open() noexcept {
    if (port_ && window_ != 0) {
        port_->setWindowOpen(window_, true);
    }
}

void NativeMenu::close() noexcept {
    if (port_ && window_ != 0) {
        port_->setWindowOpen(window_, false);
    }
}

bool NativeMenu::isOpen() const noexcept {
    return port_ && window_ != 0 && port_->isWindowOpen(window_);
}

bool NativeMenu::isRegistered() const noexcept {
    return registered_;
}

void NativeMenu::sectionCallback() noexcept {
    if (g_activeMenu && g_activeMenu->port_ && g_activeMenu->window_ != 0) {
        g_activeMenu->open();
    }
}

void NativeMenu::renderCallback() noexcept {
    if (!g_activeMenu || !g_activeMenu->render_) {
        return;
    }
    try {
        g_activeMenu->render_();
    } catch (...) {
        g_activeMenu->lastError_ = MenuRegistrationError::callbackFailed;
    }
}

}  // namespace stui::native
