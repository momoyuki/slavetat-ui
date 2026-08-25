#include "native/OfficialMenuFrameworkAdapter.h"

#include <RE/Skyrim.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <utility>
#include <vector>

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

void OfficialMenuFrameworkAdapter::renderFoundation(
    NativeCatalogBrowserModel& model,
    const std::function<void()>& close) {
    model.refresh();

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

    constexpr std::size_t searchCapacity = 256;
    std::array<char, searchCapacity> searchBuffer{};
    const auto& filter = model.filter();
    const std::size_t searchLength = std::min(filter.search.size(), searchBuffer.size() - 1);
    std::copy_n(filter.search.data(), searchLength, searchBuffer.data());
    if (ImGuiMCP::InputText("Search", searchBuffer.data(), searchBuffer.size())) {
        model.setSearch(searchBuffer.data());
    }

    const auto snapshot = model.snapshot();
    std::vector<const char*> sourceLabels{"All sources"};
    std::vector<const char*> sectionLabels{"All sections"};
    std::vector<const char*> areaLabels{"All areas"};
    int sourceIndex = 0;
    int sectionIndex = 0;
    int areaIndex = 0;

    if (snapshot) {
        const auto& facets = snapshot->repository.facets();
        for (std::size_t index = 0; index < facets.sources.size(); ++index) {
            sourceLabels.push_back(facets.sources[index].packName.c_str());
            if (facets.sources[index].sourceId == filter.sourceId) {
                sourceIndex = static_cast<int>(index + 1);
            }
        }
        for (std::size_t index = 0; index < facets.sections.size(); ++index) {
            sectionLabels.push_back(facets.sections[index].c_str());
            if (facets.sections[index] == filter.section) {
                sectionIndex = static_cast<int>(index + 1);
            }
        }
        for (std::size_t index = 0; index < facets.areas.size(); ++index) {
            areaLabels.push_back(facets.areas[index].c_str());
            if (facets.areas[index] == filter.area) {
                areaIndex = static_cast<int>(index + 1);
            }
        }
    }

    if (ImGuiMCP::Combo(
            "Source", &sourceIndex, sourceLabels.data(), static_cast<int>(sourceLabels.size()))) {
        model.setSourceId(
            sourceIndex == 0 ? "" : snapshot->repository.facets().sources[sourceIndex - 1].sourceId);
    }
    if (ImGuiMCP::Combo(
            "Section", &sectionIndex, sectionLabels.data(), static_cast<int>(sectionLabels.size()))) {
        model.setSection(
            sectionIndex == 0 ? "" : snapshot->repository.facets().sections[sectionIndex - 1]);
    }
    if (ImGuiMCP::Combo("Area", &areaIndex, areaLabels.data(), static_cast<int>(areaLabels.size()))) {
        model.setArea(areaIndex == 0 ? "" : snapshot->repository.facets().areas[areaIndex - 1]);
    }

    const auto& page = model.page();
    if (page.entries.empty()) {
        ImGuiMCP::TextUnformatted(
            snapshot ? "No tattoos match the current filters."
                     : "The tattoo catalog is empty. Refresh the catalog to browse tattoos.");
    } else {
        ImGuiMCP::Columns(2, "TattooCards", true);
        for (const auto& tattoo : page.entries) {
            ImGuiMCP::Text("Name: %s", tattoo.name.c_str());
            ImGuiMCP::Text("Pack / Source: %s / %s", tattoo.packName.c_str(), tattoo.sourceId.c_str());
            ImGuiMCP::Text("Section: %s", tattoo.section.c_str());
            ImGuiMCP::Text("Area: %s", tattoo.area.c_str());
            ImGuiMCP::Text("Texture: %s", tattoo.texturePath.c_str());
            ImGuiMCP::Separator();
            ImGuiMCP::NextColumn();
        }
        ImGuiMCP::Columns(1);
    }

    const bool hasPages = page.pageCount != 0;
    ImGuiMCP::BeginDisabled(!hasPages || page.pageIndex == 0);
    if (ImGuiMCP::Button("Prev")) {
        model.previousPage();
    }
    ImGuiMCP::EndDisabled();

    ImGuiMCP::SameLine();
    ImGuiMCP::TextUnformatted("Page");
    ImGuiMCP::SameLine();
    int pageNumber = hasPages ? static_cast<int>(page.pageIndex + 1) : 0;
    const bool committedOnEnter = ImGuiMCP::InputInt(
        "##PageNumber", &pageNumber, 0, 0, ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
    const bool committedOnDeactivate = ImGuiMCP::IsItemDeactivatedAfterEdit();
    if (hasPages && (committedOnEnter || committedOnDeactivate)) {
        model.setPageNumber(static_cast<std::size_t>(std::max(pageNumber, 1)));
    }
    ImGuiMCP::SameLine();
    ImGuiMCP::Text("/ %zu", page.pageCount);
    ImGuiMCP::SameLine();
    ImGuiMCP::BeginDisabled(!hasPages || page.pageIndex + 1 >= page.pageCount);
    if (ImGuiMCP::Button("Next")) {
        model.nextPage();
    }
    ImGuiMCP::EndDisabled();

    if (ImGuiMCP::Button("Close")) {
        open = false;
    }
    ImGuiMCP::End();
    if (!open && close) {
        close();
    }
}

}  // namespace stui::native
