#pragma once

#include "native/NativeCatalogBrowserModel.h"
#include "native/MenuFrameworkPort.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

class CatalogBrowserPageInputState {
public:
    void synchronize(std::size_t pageIndex, std::size_t pageCount) noexcept;
    [[nodiscard]] int& pendingPageNumber() noexcept;
    [[nodiscard]] std::optional<std::size_t> finishFrame(
        bool itemActive,
        bool committedOnEnter,
        bool committedOnDeactivate) noexcept;

private:
    int pendingPageNumber_{};
    int committedPageNumber_{};
    std::size_t pageCount_{};
    bool editing_{};
};

enum class CatalogBrowserEmptyState {
    none,
    emptyCatalog,
    noMatches,
};

[[nodiscard]] CatalogBrowserEmptyState classifyCatalogBrowserEmptyState(
    bool hasSnapshot,
    const repository::TattooPage& page) noexcept;
[[nodiscard]] std::string_view catalogBrowserEmptyMessage(
    CatalogBrowserEmptyState state) noexcept;

struct CatalogBrowserSourceOption {
    std::string label;
    std::string sourceId;
};

[[nodiscard]] std::vector<CatalogBrowserSourceOption> buildCatalogBrowserSourceOptions(
    const std::vector<repository::TattooSourceOption>& sources);

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
    static void renderFoundation(
        NativeCatalogBrowserModel& model,
        const std::function<void()>& close);

private:
    MenuFrameworkBindings bindings_;
    std::string section_;
};

}  // namespace stui::native
