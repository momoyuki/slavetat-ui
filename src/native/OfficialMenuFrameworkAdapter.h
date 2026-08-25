#pragma once

#include "native/NativeCatalogBrowserModel.h"
#include "native/MenuFrameworkPort.h"

#include <functional>
#include <string>

namespace stui::native {

struct MenuFrameworkBindings {
    using GetVersionFunction = float (*)();
    using AddSectionItemFunction = void (*)(const char*, MenuCallback);
    using AddWindowFunction = void* (*)(MenuCallback);
    using SetWindowOpenFunction = void (*)(void*, bool) noexcept;
    using IsWindowOpenFunction = bool (*)(const void*) noexcept;
    using SetWindowBlockingFunction = void (*)(void*, bool) noexcept;

    GetVersionFunction getVersion{};
    AddSectionItemFunction addSectionItem{};
    AddWindowFunction addWindow{};
    SetWindowOpenFunction setWindowOpen{};
    IsWindowOpenFunction isWindowOpen{};
    SetWindowBlockingFunction setWindowBlocking{};
};

struct MenuPosition {
    float x{};
    float y{};
};

struct MenuSize {
    float width{};
    float height{};
};

struct FoundationLayout {
    MenuPosition position;
    MenuSize size;
};

class OfficialMenuFrameworkAdapter final : public MenuFrameworkPort {
public:
    OfficialMenuFrameworkAdapter();
    explicit OfficialMenuFrameworkAdapter(MenuFrameworkBindings bindings);

    [[nodiscard]] bool available() const noexcept override;
    [[nodiscard]] float version() const noexcept override;
    [[nodiscard]] RegistrationResult setSection(std::string_view section) override;
    [[nodiscard]] RegistrationResult addSectionItem(
        std::string_view path, MenuCallback callback) override;
    [[nodiscard]] std::expected<MenuWindow, MenuRegistrationError> addWindow(
        MenuCallback callback, bool pauseGame) override;
    void setWindowOpen(MenuWindow window, bool open) noexcept override;
    [[nodiscard]] bool isWindowOpen(MenuWindow window) const noexcept override;

    [[nodiscard]] static FoundationLayout calculateFoundationLayout(
        MenuPosition viewportPosition, MenuSize viewportSize) noexcept;
    [[nodiscard]] static bool renderLauncher();
    static void renderFoundation(const std::function<void()>& close);
    static void renderFoundation(
        NativeCatalogBrowserModel& model,
        const std::function<void()>& close);

private:
    MenuFrameworkBindings bindings_;
    std::string section_;
};

}  // namespace stui::native
