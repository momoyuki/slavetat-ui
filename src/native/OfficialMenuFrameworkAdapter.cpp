#include "native/OfficialMenuFrameworkAdapter.h"

#include <RE/Skyrim.h>

#include <atomic>
#include <filesystem>
#include <utility>

#include "SKSEMenuFramework.h"

namespace stui::native {
namespace {

SKSEMenuFramework::Model::AddSectionItemFunction g_addSectionItem{};
SKSEMenuFramework::Model::AddWindowFunction g_addWindow{};

void addOfficialSectionItem(const char* path, MenuCallback callback) {
    g_addSectionItem(
        path, reinterpret_cast<SKSEMenuFramework::Model::RenderFunction>(callback));
}

void* addOfficialWindow(MenuCallback callback) {
    return g_addWindow(
        reinterpret_cast<SKSEMenuFramework::Model::RenderFunction>(callback));
}

void setOfficialWindowOpen(void* window, bool open) noexcept {
    static_cast<SKSEMenuFramework::Model::WindowInterface*>(window)->IsOpen = open;
}

bool isOfficialWindowOpen(const void* window) noexcept {
    return static_cast<const SKSEMenuFramework::Model::WindowInterface*>(window)->IsOpen.load();
}

void setOfficialWindowBlocking(void* window, bool blocking) noexcept {
    static_cast<SKSEMenuFramework::Model::WindowInterface*>(window)->BlockUserInput = blocking;
}

MenuFrameworkBindings resolveBindings() {
    const HMODULE module = GetModuleHandleW(L"SKSEMenuFramework.dll");
    if (!module) {
        return {};
    }
    g_addSectionItem = reinterpret_cast<SKSEMenuFramework::Model::AddSectionItemFunction>(
        GetProcAddress(module, "AddSectionItem"));
    g_addWindow = reinterpret_cast<SKSEMenuFramework::Model::AddWindowFunction>(
        GetProcAddress(module, "AddWindow"));
    return {
        reinterpret_cast<MenuFrameworkBindings::GetVersionFunction>(
            GetProcAddress(module, "GetMenuFrameworkVersion")),
        g_addSectionItem ? &addOfficialSectionItem : nullptr,
        g_addWindow ? &addOfficialWindow : nullptr,
        &setOfficialWindowOpen,
        &isOfficialWindowOpen,
        &setOfficialWindowBlocking,
    };
}

}  // namespace

OfficialMenuFrameworkAdapter::OfficialMenuFrameworkAdapter()
    : OfficialMenuFrameworkAdapter(resolveBindings()) {}

OfficialMenuFrameworkAdapter::OfficialMenuFrameworkAdapter(MenuFrameworkBindings bindings)
    : bindings_(std::move(bindings)) {}

bool OfficialMenuFrameworkAdapter::available() const noexcept {
    return bindings_.getVersion && bindings_.addSectionItem && bindings_.addWindow &&
           bindings_.setWindowOpen && bindings_.isWindowOpen && bindings_.setWindowBlocking;
}

float OfficialMenuFrameworkAdapter::version() const noexcept {
    return bindings_.getVersion ? bindings_.getVersion() : 0.0F;
}

RegistrationResult OfficialMenuFrameworkAdapter::setSection(std::string_view section) {
    if (!available()) {
        return std::unexpected(MenuRegistrationError::missingExport);
    }
    section_ = section;
    return {};
}

RegistrationResult OfficialMenuFrameworkAdapter::addSectionItem(
    std::string_view path, MenuCallback callback) {
    if (!bindings_.addSectionItem || section_.empty()) {
        return std::unexpected(MenuRegistrationError::missingExport);
    }
    const std::string fullPath = section_ + "/" + std::string(path);
    bindings_.addSectionItem(fullPath.c_str(), callback);
    return {};
}

std::expected<MenuWindow, MenuRegistrationError> OfficialMenuFrameworkAdapter::addWindow(
    MenuCallback callback, bool pauseGame) {
    if (!bindings_.addWindow || !bindings_.setWindowBlocking) {
        return std::unexpected(MenuRegistrationError::missingExport);
    }
    void* window = bindings_.addWindow(callback);
    if (!window) {
        return std::unexpected(MenuRegistrationError::windowCreationFailed);
    }
    bindings_.setWindowBlocking(window, pauseGame);
    return reinterpret_cast<MenuWindow>(window);
}

void OfficialMenuFrameworkAdapter::setWindowOpen(MenuWindow window, bool open) noexcept {
    if (bindings_.setWindowOpen && window != 0) {
        bindings_.setWindowOpen(reinterpret_cast<void*>(window), open);
    }
}

bool OfficialMenuFrameworkAdapter::isWindowOpen(MenuWindow window) const noexcept {
    return bindings_.isWindowOpen && window != 0 &&
           bindings_.isWindowOpen(reinterpret_cast<const void*>(window));
}

FoundationLayout OfficialMenuFrameworkAdapter::calculateFoundationLayout(
    MenuPosition viewportPosition, MenuSize viewportSize) noexcept {
    constexpr float margin = 20.0F;
    const MenuSize panelSize{
        viewportSize.width * 0.4F,
        viewportSize.height - (margin * 2.0F),
    };
    return {
        {
            viewportPosition.x + viewportSize.width - panelSize.width - margin,
            viewportPosition.y + margin,
        },
        panelSize,
    };
}

bool OfficialMenuFrameworkAdapter::renderLauncher() {
    ImGuiMCP::TextUnformatted("Open SlaveTatsUI when you are ready to browse tattoos.");
    return ImGuiMCP::Button("Open Tattoo Browser");
}

void OfficialMenuFrameworkAdapter::renderFoundation(const std::function<void()>& close) {
    const auto* viewport = ImGuiMCP::GetMainViewport();
    if (!viewport) {
        return;
    }
    const auto layout = calculateFoundationLayout(
        {viewport->Pos.x, viewport->Pos.y},
        {viewport->Size.x, viewport->Size.y});
    ImGuiMCP::SetNextWindowPos(
        {layout.position.x, layout.position.y}, ImGuiMCP::ImGuiCond_Appearing, {0.0F, 0.0F});
    ImGuiMCP::SetNextWindowSize(
        {layout.size.width, layout.size.height}, ImGuiMCP::ImGuiCond_Appearing);
    bool open = true;
    ImGuiMCP::Begin(
        "Tattoo Browser##SlaveTatsUI", &open, ImGuiMCP::ImGuiWindowFlags_NoCollapse);
    ImGuiMCP::TextUnformatted("Native menu foundation is ready.");
    ImGuiMCP::TextUnformatted("Catalog browsing remains available through PrismaUI (F8).");
    if (ImGuiMCP::Button("Close")) {
        open = false;
    }
    ImGuiMCP::End();
    if (!open && close) {
        close();
    }
}

}  // namespace stui::native
