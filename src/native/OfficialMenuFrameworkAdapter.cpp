#include "native/OfficialMenuFrameworkAdapter.h"

#include <RE/Skyrim.h>

#include "native/NativeThumbnailRuntime.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <limits>
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

CatalogThumbnailFit fitCatalogThumbnail(
    std::size_t textureWidth,
    std::size_t textureHeight,
    float maximumWidth,
    float maximumHeight) noexcept {
    if (textureWidth == 0 || textureHeight == 0 || maximumWidth <= 0.0F ||
        maximumHeight <= 0.0F) {
        return {};
    }

    const float width = static_cast<float>(textureWidth);
    const float height = static_cast<float>(textureHeight);
    const float scale = std::min(maximumWidth / width, maximumHeight / height);
    return {.width = width * scale, .height = height * scale};
}

CatalogCardGridPosition catalogCardGridPosition(
    std::size_t index,
    std::size_t columnCount) noexcept {
    if (columnCount == 0) {
        return {};
    }
    return {.row = index / columnCount, .column = index % columnCount};
}

std::string catalogCardWidgetId(
    std::string_view role,
    std::size_t index) {
    return std::string(role) + "##" + std::to_string(index);
}

CatalogBrowserGridLayout calculateCatalogBrowserGridLayout(
    float availableHeight,
    float footerHeight,
    float metadataHeight,
    std::size_t rowCount) noexcept {
    if (rowCount == 0) {
        return {};
    }

    const float gridHeight = std::max(0.0F, availableHeight - footerHeight);
    const float rowHeight = gridHeight / static_cast<float>(rowCount);
    return {
        .gridHeight = gridHeight,
        .rowHeight = rowHeight,
        .thumbnailHeight = std::max(0.0F, rowHeight - metadataHeight),
    };
}

CatalogAreaBadgeLayout calculateCatalogAreaBadgeLayout(
    float containerWidth,
    float textWidth,
    float textHeight,
    float horizontalPadding,
    float verticalPadding,
    float margin) noexcept {
    const float width = std::max(0.0F, textWidth + horizontalPadding * 2.0F);
    const float height = std::max(0.0F, textHeight + verticalPadding * 2.0F);
    const float x = std::max(0.0F, containerWidth - margin - width);
    const float y = std::max(0.0F, margin);
    return {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .textX = x + horizontalPadding,
        .textY = y + verticalPadding,
    };
}

float calculateCatalogCardMetadataHeight(
    float,
    float) noexcept {
    return 0.0F;
}

float calculateRightAlignedControlX(
    float availableWidth,
    float controlWidth) noexcept {
    return std::max(0.0F, availableWidth - controlWidth);
}

namespace {

char canonicalThumbnailPathCharacter(char character) noexcept {
    if (character == '\\') {
        return '/';
    }
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character - 'A' + 'a');
    }
    return character;
}

bool equivalentThumbnailPaths(std::string_view left, std::string_view right) noexcept {
    return left.size() == right.size() && std::ranges::equal(
        left,
        right,
        {},
        canonicalThumbnailPathCharacter,
        canonicalThumbnailPathCharacter);
}

}  // namespace

std::string_view catalogThumbnailStatusLabel(NativeThumbnailStatus status) noexcept {
    switch (status) {
    case NativeThumbnailStatus::loading:
        return "Loading";
    case NativeThumbnailStatus::missing:
        return "Missing";
    case NativeThumbnailStatus::broken:
        return "Broken";
    case NativeThumbnailStatus::placeholder:
    case NativeThumbnailStatus::ready:
        return {};
    }
    return {};
}

std::optional<std::size_t> findCatalogThumbnailViewIndex(
    std::string_view texturePath,
    const std::vector<NativeThumbnailView>& thumbnailViews) noexcept {
    for (std::size_t index = 0; index < thumbnailViews.size(); ++index) {
        if (equivalentThumbnailPaths(texturePath, thumbnailViews[index].texturePath)) {
            return index;
        }
    }
    return std::nullopt;
}

void CatalogBrowserPageInputState::synchronize(
    std::size_t pageIndex,
    std::size_t pageCount) noexcept {
    const std::size_t oneBasedPage = pageCount == 0 ? 0 : pageIndex + 1;
    const int committedPageNumber = static_cast<int>(std::min(
        oneBasedPage,
        static_cast<std::size_t>(std::numeric_limits<int>::max())));
    if (!editing_ || committedPageNumber != committedPageNumber_ || pageCount != pageCount_) {
        pendingPageNumber_ = committedPageNumber;
    }
    committedPageNumber_ = committedPageNumber;
    pageCount_ = pageCount;
    if (pageCount_ == 0) {
        editing_ = false;
    }
}

int& CatalogBrowserPageInputState::pendingPageNumber() noexcept {
    return pendingPageNumber_;
}

std::optional<std::size_t> CatalogBrowserPageInputState::finishFrame(
    bool itemActive,
    bool committedOnEnter,
    bool committedOnDeactivate) noexcept {
    if (pageCount_ == 0) {
        editing_ = false;
        return std::nullopt;
    }
    if (committedOnEnter || committedOnDeactivate) {
        editing_ = false;
        return pendingPageNumber_ > 0 ? static_cast<std::size_t>(pendingPageNumber_) : 0;
    }
    editing_ = itemActive;
    return std::nullopt;
}

CatalogBrowserEmptyState classifyCatalogBrowserEmptyState(
    bool hasSnapshot,
    const repository::TattooPage& page) noexcept {
    if (!hasSnapshot || page.totalEntries == 0) {
        return CatalogBrowserEmptyState::emptyCatalog;
    }
    if (page.matchedEntries == 0) {
        return CatalogBrowserEmptyState::noMatches;
    }
    return CatalogBrowserEmptyState::none;
}

std::string_view catalogBrowserEmptyMessage(CatalogBrowserEmptyState state) noexcept {
    switch (state) {
    case CatalogBrowserEmptyState::emptyCatalog:
        return "The tattoo catalog is empty. Refresh the catalog to browse tattoos.";
    case CatalogBrowserEmptyState::noMatches:
        return "No tattoos match the current filters.";
    case CatalogBrowserEmptyState::none:
        return {};
    }
    return {};
}

std::vector<CatalogBrowserSourceOption> buildCatalogBrowserSourceOptions(
    const std::vector<repository::TattooSourceOption>& sources) {
    std::vector<CatalogBrowserSourceOption> options;
    options.reserve(sources.size());
    for (const auto& source : sources) {
        options.push_back({
            .label = source.packName + " (" + source.sourceId + ")",
            .sourceId = source.sourceId,
        });
    }
    return options;
}

bool OfficialMenuFrameworkAdapter::renderLauncher() {
    ImGuiMCP::TextUnformatted("Open SlaveTatsUI when you are ready to browse tattoos.");
    return ImGuiMCP::Button("Open Tattoo Browser");
}

void OfficialMenuFrameworkAdapter::renderFoundation(
    NativeCatalogBrowserModel& model,
    NativeThumbnailRuntime& thumbnails,
    const std::function<void()>& close) {
    model.refresh();
    thumbnails.synchronize(model.snapshot(), model.page());
    thumbnails.pump();
    const auto thumbnailViews = thumbnails.views();

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
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGuiMCP::Begin(
        "Tattoo Browser##SlaveTatsUI",
        &open,
        ImGuiMCP::ImGuiWindowFlags_NoCollapse |
            ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
            ImGuiMCP::ImGuiWindowFlags_NoScrollWithMouse);

    static bool filtersExpanded = false;
    if (ImGuiMCP::Button(filtersExpanded ? "Hide filters" : "Filters")) {
        filtersExpanded = !filtersExpanded;
    }

    constexpr std::size_t searchCapacity = 256;
    std::array<char, searchCapacity> searchBuffer{};
    const auto& filter = model.filter();
    const std::size_t searchLength = std::min(filter.search.size(), searchBuffer.size() - 1);
    std::copy_n(filter.search.data(), searchLength, searchBuffer.data());
    const auto snapshot = model.snapshot();
    std::vector<CatalogBrowserSourceOption> sourceOptions;
    std::vector<const char*> sectionLabels{"All sections"};
    std::vector<const char*> areaLabels{"All areas"};
    int sourceIndex = 0;
    int sectionIndex = 0;
    int areaIndex = 0;

    if (snapshot) {
        const auto& facets = snapshot->repository.facets();
        sourceOptions = buildCatalogBrowserSourceOptions(facets.sources);
        for (std::size_t index = 0; index < facets.sources.size(); ++index) {
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

    std::vector<const char*> sourceLabels{"All sources"};
    sourceLabels.reserve(sourceOptions.size() + 1);
    for (const auto& source : sourceOptions) {
        sourceLabels.push_back(source.label.c_str());
    }

    if (filtersExpanded && ImGuiMCP::BeginTable(
            "FilterControls",
            2,
            ImGuiMCP::ImGuiTableFlags_SizingStretchProp)) {
        ImGuiMCP::TableSetupColumn(
            "FilterLabel", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 72.0F);
        ImGuiMCP::TableSetupColumn(
            "FilterControl", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted("Search");
        ImGuiMCP::TableSetColumnIndex(1);
        ImGuiMCP::SetNextItemWidth(-1.0F);
        if (ImGuiMCP::InputText("##Search", searchBuffer.data(), searchBuffer.size())) {
            model.setSearch(searchBuffer.data());
        }

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted("Source");
        ImGuiMCP::TableSetColumnIndex(1);
        ImGuiMCP::SetNextItemWidth(-1.0F);
        if (ImGuiMCP::Combo(
                "##Source",
                &sourceIndex,
                sourceLabels.data(),
                static_cast<int>(sourceLabels.size()))) {
            model.setSourceId(
                sourceIndex == 0 ? "" : sourceOptions[sourceIndex - 1].sourceId);
        }

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted("Section");
        ImGuiMCP::TableSetColumnIndex(1);
        ImGuiMCP::SetNextItemWidth(-1.0F);
        if (ImGuiMCP::Combo(
                "##Section",
                &sectionIndex,
                sectionLabels.data(),
                static_cast<int>(sectionLabels.size()))) {
            model.setSection(
                sectionIndex == 0 ? "" : snapshot->repository.facets().sections[sectionIndex - 1]);
        }

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted("Area");
        ImGuiMCP::TableSetColumnIndex(1);
        ImGuiMCP::SetNextItemWidth(-1.0F);
        if (ImGuiMCP::Combo(
                "##Area", &areaIndex, areaLabels.data(), static_cast<int>(areaLabels.size()))) {
            model.setArea(
                areaIndex == 0 ? "" : snapshot->repository.facets().areas[areaIndex - 1]);
        }
        ImGuiMCP::EndTable();
    }

    const auto& page = model.page();
    const auto emptyState = classifyCatalogBrowserEmptyState(snapshot != nullptr, page);
    const auto* style = ImGuiMCP::GetStyle();
    const float itemSpacing = style ? style->ItemSpacing.y : 4.0F;
    const float metadataHeight = calculateCatalogCardMetadataHeight(
        ImGuiMCP::GetTextLineHeightWithSpacing(), itemSpacing);
    const float footerHeight = ImGuiMCP::GetFrameHeightWithSpacing();
    const auto gridLayout = calculateCatalogBrowserGridLayout(
        ImGuiMCP::GetContentRegionAvail().y,
        footerHeight,
        metadataHeight,
        3);
    if (emptyState != CatalogBrowserEmptyState::none) {
        if (ImGuiMCP::BeginChild(
                "CatalogEmptyState",
                {0.0F, gridLayout.gridHeight},
                ImGuiMCP::ImGuiChildFlags_None,
                ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
                    ImGuiMCP::ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGuiMCP::TextUnformatted(catalogBrowserEmptyMessage(emptyState).data());
        }
        ImGuiMCP::EndChild();
    } else {
        constexpr std::size_t columnCount = 2;
        constexpr auto tableFlags =
            ImGuiMCP::ImGuiTableFlags_SizingStretchSame |
            ImGuiMCP::ImGuiTableFlags_BordersInner |
            ImGuiMCP::ImGuiTableFlags_NoPadOuterX |
            ImGuiMCP::ImGuiTableFlags_NoPadInnerX;
        if (ImGuiMCP::BeginTable(
                "TattooCards",
                static_cast<int>(columnCount),
                tableFlags,
                {0.0F, gridLayout.gridHeight})) {
            for (std::size_t index = 0; index < page.entries.size(); ++index) {
                const auto gridPosition = catalogCardGridPosition(index, columnCount);
                if (gridPosition.column == 0) {
                    ImGuiMCP::TableNextRow(0, gridLayout.rowHeight);
                }
                ImGuiMCP::TableSetColumnIndex(static_cast<int>(gridPosition.column));
                const auto& tattoo = page.entries[index];
                const auto thumbnailIndex = findCatalogThumbnailViewIndex(
                    tattoo.texturePath,
                    thumbnailViews);
                const NativeThumbnailView* thumbnail = thumbnailIndex
                    ? &thumbnailViews[*thumbnailIndex]
                    : nullptr;

                const auto thumbnailWidgetId = catalogCardWidgetId("Thumbnail", index);

                ImGuiMCP::PushStyleColor(
                    ImGuiMCP::ImGuiCol_ChildBg,
                    ImGuiMCP::ImVec4(0.08F, 0.08F, 0.08F, 1.0F));
                if (ImGuiMCP::BeginChild(
                        thumbnailWidgetId.c_str(),
                        {0.0F, std::max(1.0F, gridLayout.thumbnailHeight)},
                        ImGuiMCP::ImGuiChildFlags_Border,
                            ImGuiMCP::ImGuiWindowFlags_NoScrollbar |
                            ImGuiMCP::ImGuiWindowFlags_NoScrollWithMouse)) {
                    const auto imageRegion = ImGuiMCP::GetContentRegionAvail();
                    const auto imageOrigin = ImGuiMCP::GetCursorPos();
                    const auto imageScreenOrigin = ImGuiMCP::GetCursorScreenPos();
                    if (thumbnail && thumbnail->status == NativeThumbnailStatus::ready &&
                        thumbnail->texture && thumbnail->texture->shaderResourceView) {
                        const auto texture = thumbnail->texture;
                        const auto fit = fitCatalogThumbnail(
                            texture->width,
                            texture->height,
                            imageRegion.x,
                            imageRegion.y);
                        ImGuiMCP::SetCursorPos({
                            imageOrigin.x + (imageRegion.x - fit.width) / 2.0F,
                            imageOrigin.y + (imageRegion.y - fit.height) / 2.0F,
                        });
                        ImGuiMCP::Image(
                            static_cast<ImGuiMCP::ImTextureID>(
                                texture->shaderResourceView.get()),
                            {fit.width, fit.height});
                    } else if (thumbnail) {
                        const auto label = catalogThumbnailStatusLabel(thumbnail->status);
                        if (!label.empty()) {
                            ImGuiMCP::TextUnformatted(label.data());
                        }
                    }

                    if (!tattoo.area.empty()) {
                        const auto textSize = ImGuiMCP::CalcTextSize(tattoo.area.c_str());
                        const auto badge = calculateCatalogAreaBadgeLayout(
                            imageRegion.x,
                            textSize.x,
                            textSize.y,
                            6.0F,
                            3.0F,
                            4.0F);
                        auto* drawList = ImGuiMCP::GetWindowDrawList();
                        ImGuiMCP::ImDrawListManager::AddRectFilled(
                            drawList,
                            {
                                imageScreenOrigin.x + badge.x,
                                imageScreenOrigin.y + badge.y,
                            },
                            {
                                imageScreenOrigin.x + badge.x + badge.width,
                                imageScreenOrigin.y + badge.y + badge.height,
                            },
                            0xB8000000,
                            3.0F,
                            0);
                        ImGuiMCP::ImDrawListManager::AddText(
                            drawList,
                            {
                                imageScreenOrigin.x + badge.textX,
                                imageScreenOrigin.y + badge.textY,
                            },
                            0xFFFFFFFF,
                            tattoo.area.c_str());
                    }
                }
                ImGuiMCP::EndChild();
                ImGuiMCP::PopStyleColor();
                if (ImGuiMCP::IsItemHovered()) {
                    ImGuiMCP::SetTooltip("%s", tattoo.name.c_str());
                }
            }
            ImGuiMCP::EndTable();
        }
    }

    const float closeButtonWidth = ImGuiMCP::CalcTextSize("Close").x +
        (style ? style->FramePadding.x * 2.0F : 16.0F);
    if (ImGuiMCP::BeginTable(
            "CatalogFooter",
            2,
            ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                ImGuiMCP::ImGuiTableFlags_NoPadOuterX)) {
        ImGuiMCP::TableSetupColumn(
            "Pagination", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableSetupColumn(
            "CloseAction",
            ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
            closeButtonWidth);
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);

        const bool hasPages = page.pageCount != 0;
        ImGuiMCP::BeginDisabled(!hasPages || page.pageIndex == 0);
        if (ImGuiMCP::Button("Prev")) {
            model.previousPage();
        }
        ImGuiMCP::EndDisabled();

        ImGuiMCP::SameLine();
        ImGuiMCP::TextUnformatted("Page");
        ImGuiMCP::SameLine();
        static CatalogBrowserPageInputState pageInputState;
        pageInputState.synchronize(page.pageIndex, page.pageCount);
        ImGuiMCP::SetNextItemWidth(56.0F);
        ImGuiMCP::InputInt(
            "##PageNumber",
            &pageInputState.pendingPageNumber(),
            0,
            0);
        const bool itemActive = ImGuiMCP::IsItemActive();
        const bool committedOnDeactivate = ImGuiMCP::IsItemDeactivatedAfterEdit();
        if (const auto requestedPage = pageInputState.finishFrame(
                itemActive, false, committedOnDeactivate)) {
            model.setPageNumber(*requestedPage);
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::Text("/ %zu", page.pageCount);
        ImGuiMCP::SameLine();
        ImGuiMCP::BeginDisabled(!hasPages || page.pageIndex + 1 >= page.pageCount);
        if (ImGuiMCP::Button("Next")) {
            model.nextPage();
        }
        ImGuiMCP::EndDisabled();

        ImGuiMCP::TableSetColumnIndex(1);
        const float closeOffset = calculateRightAlignedControlX(
            ImGuiMCP::GetContentRegionAvail().x,
            closeButtonWidth);
        ImGuiMCP::SetCursorPosX(ImGuiMCP::GetCursorPosX() + closeOffset);
        if (ImGuiMCP::Button("Close")) {
            open = false;
        }
        ImGuiMCP::EndTable();
    }
    ImGuiMCP::End();
    ImGuiMCP::PopStyleVar();
    if (!open && close) {
        close();
    }
}

}  // namespace stui::native
