#include "native/OfficialMenuFrameworkAdapter.h"
#include "native/NativeCatalogBrowserModel.h"
#include "native/NativeThumbnailController.h"
#include "native/NativeSlotWorkflowModel.h"
#include "native/NativeSlotWorkflowRuntime.h"
#include "core/TattooModels.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

float returnVersionThree() {
    return 3.13F;
}

struct FakeWindow {
    bool open{};
    bool blocking{true};
};

FakeWindow g_window;
std::string g_itemPath;
stui::native::MenuCallback g_itemCallback{};

void addSectionItem(const char* path, stui::native::MenuCallback callback) {
    g_itemPath = path;
    g_itemCallback = callback;
}

void* addWindow(stui::native::MenuCallback) {
    return &g_window;
}

void setWindowOpen(void* window, bool open) noexcept {
    static_cast<FakeWindow*>(window)->open = open;
}

bool isWindowOpen(const void* window) noexcept {
    return static_cast<const FakeWindow*>(window)->open;
}

void setWindowBlocking(void* window, bool blocking) noexcept {
    static_cast<FakeWindow*>(window)->blocking = blocking;
}

stui::native::MenuFrameworkBindings completeBindings() {
    return {
        &returnVersionThree,
        &addSectionItem,
        &addWindow,
        &setWindowOpen,
        &isWindowOpen,
        &setWindowBlocking,
    };
}

void rejectsIncompleteExportTable() {
    stui::native::MenuFrameworkBindings bindings{};
    bindings.getVersion = &returnVersionThree;

    const stui::native::OfficialMenuFrameworkAdapter adapter(bindings);

    expect(!adapter.available(), "expected incomplete export table rejection");
}

void translatesSectionAndNonPausingWindowState() {
    g_window = {};
    g_window.blocking = true;
    g_itemPath.clear();
    g_itemCallback = nullptr;
    stui::native::OfficialMenuFrameworkAdapter adapter(completeBindings());

    expect(adapter.available(), "expected complete bindings");
    expect(adapter.setSection("SlaveTatsUI").has_value(), "expected section setup");
    auto window = adapter.addWindow(nullptr, false);
    expect(window.has_value(), "expected opaque window token");
    expect(adapter.addSectionItem("Tattoo Browser", nullptr).has_value(),
           "expected browser item registration");
    adapter.setWindowOpen(*window, true);

    expect(g_itemPath == "SlaveTatsUI/Tattoo Browser", "expected composed item path");
    expect(!g_window.blocking, "expected non-pausing window");
    expect(adapter.isWindowOpen(*window), "expected translated open state");
}

void defaultAdapterIsUnavailableWithoutLoadedFramework() {
    const stui::native::OfficialMenuFrameworkAdapter adapter;

    expect(!adapter.available(), "expected absent runtime framework to remain optional");
    expect(adapter.version() == 0.0F, "expected zero version without loaded framework");
}

void foundationLayoutAnchorsFortyPercentPanelToRightEdge() {
    const auto layout = stui::native::OfficialMenuFrameworkAdapter::calculateFoundationLayout(
        {10.0F, 20.0F}, {1000.0F, 800.0F});

    expect(layout.position.x == 590.0F && layout.position.y == 40.0F,
           "expected right-edge position with margin");
    expect(layout.size.width == 400.0F && layout.size.height == 760.0F,
           "expected forty-percent full-height side panel");
}

void thumbnailPresentationFitsWithoutStretchingAndLabelsFailures() {
    const auto wide = stui::native::fitCatalogThumbnail(400, 200, 180.0F, 140.0F);
    expect(wide.width == 180.0F && wide.height == 90.0F,
           "expected wide texture fitted without stretching");

    const auto tall = stui::native::fitCatalogThumbnail(100, 400, 180.0F, 140.0F);
    expect(tall.width == 35.0F && tall.height == 140.0F,
           "expected tall texture fitted without stretching");

    expect(stui::native::catalogThumbnailStatusLabel(
               stui::native::NativeThumbnailStatus::loading) == "Loading",
           "expected explicit loading label");
    expect(stui::native::catalogThumbnailStatusLabel(
               stui::native::NativeThumbnailStatus::missing) == "Missing",
           "expected explicit missing label");
    expect(stui::native::catalogThumbnailStatusLabel(
               stui::native::NativeThumbnailStatus::broken) == "Broken",
           "expected explicit broken label");
    expect(stui::native::catalogThumbnailStatusLabel(
               stui::native::NativeThumbnailStatus::ready).empty(),
           "expected ready texture without a status label");
    expect(stui::native::catalogThumbnailStatusLabel(
               stui::native::NativeThumbnailStatus::placeholder).empty(),
           "expected placeholder without a status label");
}

void thumbnailViewLookupMapsDuplicatePathVariantsToOneView() {
    const std::vector<stui::native::NativeThumbnailView> views{
        {.texturePath = "textures/actors/character/slavetats/a.dds"},
        {.texturePath = "textures/actors/character/slavetats/b.dds"},
    };

    expect(stui::native::findCatalogThumbnailViewIndex(
               "textures/actors/character/slavetats/a.dds", views) == 0,
           "expected first entry to map to the first thumbnail view");
    expect(stui::native::findCatalogThumbnailViewIndex(
               "TEXTURES\\ACTORS\\CHARACTER\\SLAVETATS\\A.DDS", views) == 0,
           "expected slash and case variant to reuse the first thumbnail view");
    expect(stui::native::findCatalogThumbnailViewIndex(
               "textures/actors/character/slavetats/b.dds", views) == 1,
           "expected third entry to map to the second unique thumbnail view");
}

void thumbnailGridGroupsTwoCardsIntoEachRow() {
    const auto first = stui::native::catalogCardGridPosition(0, 2);
    const auto second = stui::native::catalogCardGridPosition(1, 2);
    const auto third = stui::native::catalogCardGridPosition(2, 2);

    expect(first.row == 0 && first.column == 0,
        "expected first card in row zero column zero");
    expect(second.row == 0 && second.column == 1,
        "expected second card beside first card in row zero");
    expect(third.row == 1 && third.column == 0,
        "expected third card at the start of row one");
}

void thumbnailCardWidgetsHaveStableUniqueIds() {
    expect(stui::native::catalogCardWidgetId("Thumbnail", "source-a.json", 7) ==
               "Thumbnail##source-a.json:7",
        "expected thumbnail ID derived from stable catalog identity");
    expect(stui::native::catalogCardWidgetId("Thumbnail", "source-a.json", 7) !=
               stui::native::catalogCardWidgetId("Thumbnail", "source-b.json", 7),
        "expected equal source indices from different files to remain unique");
}

void browserGridUsesRemainingHeightWithoutVerticalScrolling() {
    const auto collapsed = stui::native::calculateCatalogBrowserGridLayout(
        700.0F, 40.0F, 60.0F, 3);
    expect(collapsed.gridHeight == 660.0F,
        "expected footer height reserved below the collapsed-filter grid");
    expect(collapsed.rowHeight == 220.0F && collapsed.thumbnailHeight == 160.0F,
        "expected three equal image-first rows with collapsed filters");

    const auto expanded = stui::native::calculateCatalogBrowserGridLayout(
        520.0F, 40.0F, 60.0F, 3);
    expect(expanded.gridHeight == 480.0F,
        "expected expanded filters to leave a smaller bounded grid");
    expect(expanded.rowHeight == 160.0F && expanded.thumbnailHeight == 100.0F,
        "expected all three rows to remain visible with expanded filters");

    const auto constrained = stui::native::calculateCatalogBrowserGridLayout(
        80.0F, 40.0F, 60.0F, 3);
    expect(constrained.thumbnailHeight == 0.0F,
        "expected small windows to clamp thumbnail height instead of going negative");
}

void catalogBadgeAnchorsInsideThumbnailTopRightCorner() {
    const auto badge = stui::native::calculateCatalogBadgeLayout(
        200.0F, 40.0F, 16.0F, 6.0F, 3.0F, 4.0F);

    expect(badge.x == 144.0F && badge.y == 4.0F,
        "expected area badge anchored four pixels from the thumbnail top-right");
    expect(badge.width == 52.0F && badge.height == 22.0F,
        "expected area badge padding around its text");
    expect(badge.textX == 150.0F && badge.textY == 7.0F,
        "expected area text inset inside the badge background");
}

void thumbnailCardsReserveNoPersistentMetadataRow() {
    const float metadataHeight =
        stui::native::calculateCatalogCardMetadataHeight(24.0F, 4.0F);
    expect(metadataHeight == 0.0F,
        "expected thumbnail cards to reserve no persistent name row");

    const auto layout = stui::native::calculateCatalogBrowserGridLayout(
        700.0F, 40.0F, metadataHeight, 3);
    expect(layout.thumbnailHeight == 220.0F,
        "expected each thumbnail to use the full available row height");
}

void footerControlAlignsToRightContentEdge() {
    expect(stui::native::calculateRightAlignedControlX(300.0F, 64.0F) == 236.0F,
        "expected close button aligned to the footer right edge");
    expect(stui::native::calculateRightAlignedControlX(40.0F, 64.0F) == 0.0F,
        "expected constrained footer alignment clamped inside its cell");
}

void pickerFooterActionsStayRightAlignedInNavigationOrder() {
    const auto layout = stui::native::calculatePickerFooterActionLayout(
        200.0F,
        60.0F,
        50.0F,
        8.0F);

    expect(layout.groupWidth == 118.0F,
        "expected Cancel and Close widths plus one spacing interval");
    expect(layout.cancelX == 82.0F,
        "expected footer action group aligned to the right edge");
    expect(layout.closeX == 150.0F,
        "expected Close after Cancel in navigation order");
}

void tattooColorComponentsPreserveRgbChannelOrder() {
    const auto components = stui::native::tattooColorComponents(0x804020);

    expect(components.red == 128.0F / 255.0F &&
            components.green == 64.0F / 255.0F &&
            components.blue == 32.0F / 255.0F,
        "expected 0xRRGGBB unpacked without swapping color channels");
    expect(stui::native::tattooColorValue({1.0F, 128.0F / 255.0F, 0.0F}) ==
            0xFF8000,
        "expected RGB picker components rounded into 0xRRGGBB");
}

void currentSlotColorSwatchUsesOwnedTattooColorAtBottomRight() {
    const stui::core::TattooSlot owned{
        .index = 2,
        .occupancy = stui::core::SlotOccupancy::slaveTats,
        .tattoo = stui::core::TattooEntry{.color = 0x123456},
    };

    const auto swatch = stui::native::calculateSlotColorSwatch(
        owned, 200.0F, 120.0F, 16.0F, 6.0F);

    expect(swatch.has_value(), "expected owned Current Slot to expose a color swatch");
    expect(swatch->x == 178.0F && swatch->y == 98.0F && swatch->size == 16.0F,
        "expected color swatch anchored inside the thumbnail bottom-right corner");
    expect(swatch->fillColor == 0xFF563412U,
        "expected 0xRRGGBB tattoo color packed for ImGui without swapped channels");
    expect(swatch->borderColor == 0xFF202020U,
        "expected a dark opaque outline around the color swatch");
}

void currentSlotColorSwatchSkipsEmptyAndExternalSlots() {
    const stui::core::TattooSlot empty{};
    const stui::core::TattooSlot external{
        .index = 3,
        .occupancy = stui::core::SlotOccupancy::external,
        .tattoo = stui::core::TattooEntry{.color = 0xABCDEF},
    };

    expect(!stui::native::calculateSlotColorSwatch(
                empty, 200.0F, 120.0F, 16.0F, 6.0F).has_value(),
        "expected empty Current Slot to omit the color swatch");
    expect(!stui::native::calculateSlotColorSwatch(
                external, 200.0F, 120.0F, 16.0F, 6.0F).has_value(),
        "expected external Current Slot to omit the color swatch");
}

void catalogTooltipIncludesInUseSlotDetails() {
    expect(stui::native::formatCatalogTattooTooltip("Corruption", {}) == "Corruption",
        "expected unused tattoo tooltip to contain only its name");
    expect(stui::native::formatCatalogTattooTooltip("Corruption", {2, 5}) ==
            "Corruption\nIn use: Slots 2, 5",
        "expected In Use tooltip to list every matching Current Slot");
}

void pageInputKeepsPendingEditsUntilEnterOrFocusLoss() {
    stui::native::CatalogBrowserPageInputState state;
    state.synchronize(1, 5);
    expect(state.pendingPageNumber() == 2, "expected one-based committed page value");

    state.pendingPageNumber() = 4;
    expect(!state.finishFrame(true, false, false).has_value(),
           "expected active edit not to commit on a keystroke");
    state.synchronize(1, 5);
    expect(state.pendingPageNumber() == 4,
           "expected pending edit to survive the next frame");

    const auto enterCommit = state.finishFrame(true, true, false);
    expect(enterCommit == 4, "expected Enter to commit the edited one-based value");
    state.synchronize(3, 5);
    expect(state.pendingPageNumber() == 4,
           "expected committed model page to resynchronize the input");

    state.pendingPageNumber() = 5;
    expect(!state.finishFrame(true, false, false).has_value(),
           "expected second pending edit not to commit early");
    const auto focusLossCommit = state.finishFrame(false, false, true);
    expect(focusLossCommit == 5,
           "expected focus loss to commit the edited one-based value");

    state.synchronize(0, 5);
    state.pendingPageNumber() = -7;
    expect(state.finishFrame(false, true, false) == 0,
           "expected invalid low input to reach model clamping as zero");
    state.synchronize(0, 0);
    state.pendingPageNumber() = 3;
    expect(!state.finishFrame(false, true, false).has_value(),
           "expected empty pagination not to commit");
}

void classifiesEmptyCatalogSeparatelyFromNoMatches() {
    const stui::repository::TattooPage validEmptyCatalog{
        .totalEntries = 0,
        .matchedEntries = 0,
        .pageCount = 0,
    };
    const auto emptyState =
        stui::native::classifyCatalogBrowserEmptyState(true, validEmptyCatalog);
    expect(emptyState == stui::native::CatalogBrowserEmptyState::emptyCatalog,
           "expected a valid zero-entry snapshot to be an empty catalog");
    expect(stui::native::catalogBrowserEmptyMessage(emptyState) ==
               "The tattoo catalog is empty. Refresh the catalog to browse tattoos.",
           "expected explicit empty-catalog message");

    const stui::repository::TattooPage noMatches{
        .totalEntries = 3,
        .matchedEntries = 0,
        .pageCount = 0,
    };
    const auto noMatchState =
        stui::native::classifyCatalogBrowserEmptyState(true, noMatches);
    expect(noMatchState == stui::native::CatalogBrowserEmptyState::noMatches,
           "expected filtered zero matches from a non-empty catalog");
    expect(stui::native::catalogBrowserEmptyMessage(noMatchState) ==
               "No tattoos match the current filters.",
           "expected explicit no-match message");

    expect(stui::native::classifyCatalogBrowserEmptyState(false, noMatches) ==
               stui::native::CatalogBrowserEmptyState::emptyCatalog,
           "expected no snapshot to remain an empty-catalog state");
}

void sourceOptionsDistinguishDuplicatePackNamesAndPreserveIds() {
    const std::vector<stui::repository::TattooSourceOption> sources{
        {.sourceId = "source-a.json", .packName = "Shared Pack"},
        {.sourceId = "source-b.json", .packName = "Shared Pack"},
    };

    const auto options = stui::native::buildCatalogBrowserSourceOptions(sources);
    expect(options.size() == 2, "expected one presentation option per source");
    expect(options[0].label == "Shared Pack (source-a.json)" &&
               options[1].label == "Shared Pack (source-b.json)",
           "expected duplicate pack names to include distinct source IDs");
    expect(options[0].sourceId == "source-a.json" &&
               options[1].sourceId == "source-b.json",
           "expected selection payloads to preserve exact source IDs");
}

void slotCardsExposeAddReplaceAndDisabledTreatments() {
    expect(stui::native::slotCardTreatment(stui::core::SlotOccupancy::empty) ==
               stui::native::SlotCardTreatment::add,
        "expected empty slot Add treatment");
    expect(stui::native::slotCardTreatment(stui::core::SlotOccupancy::slaveTats) ==
               stui::native::SlotCardTreatment::replace,
        "expected SlaveTats slot Replace treatment");
    expect(stui::native::slotCardTreatment(stui::core::SlotOccupancy::external) ==
               stui::native::SlotCardTreatment::disabled,
        "expected external slot disabled treatment");
}

void slotPaginationClampsToSixCardPages() {
    const auto first = stui::native::calculateSlotPage(12, 0, 6);
    expect(first.pageIndex == 0 && first.pageCount == 2 && first.begin == 0 &&
            first.end == 6,
        "expected first BODY page to contain six slots");

    const auto clamped = stui::native::calculateSlotPage(13, 9, 6);
    expect(clamped.pageIndex == 2 && clamped.pageCount == 3 &&
            clamped.begin == 12 && clamped.end == 13,
        "expected requested page clamped to the final partial page");

    const auto empty = stui::native::calculateSlotPage(0, 3, 6);
    expect(empty.pageIndex == 0 && empty.pageCount == 0 && empty.begin == 0 &&
            empty.end == 0,
        "expected empty slot collection to have safe zero bounds");
}

void slotAreaLabelsMatchSlaveTatsAreaNames() {
    expect(stui::native::slotAreaLabel(stui::core::TattooArea::body) == "BODY",
        "expected BODY tab label");
    expect(stui::native::slotAreaLabel(stui::core::TattooArea::face) == "FACE",
        "expected FACE tab label");
    expect(stui::native::slotAreaLabel(stui::core::TattooArea::hands) == "HANDS",
        "expected HANDS tab label");
    expect(stui::native::slotAreaLabel(stui::core::TattooArea::feet) == "FEET",
        "expected FEET tab label");
}

void visibleSlotPathsIncludeOnlyOwnedCardsOnTheCurrentPage() {
    const stui::core::TattooSlots slots{
        .actorFormId = 0x14,
        .area = stui::core::TattooArea::body,
        .configuredCount = 8,
        .slots = {
            {.index = 0, .occupancy = stui::core::SlotOccupancy::empty},
            {.index = 1, .occupancy = stui::core::SlotOccupancy::external},
            {.index = 2,
                .occupancy = stui::core::SlotOccupancy::slaveTats,
                .tattoo = stui::core::TattooEntry{.texturePath = "owned/a.dds"}},
            {.index = 3,
                .occupancy = stui::core::SlotOccupancy::slaveTats,
                .tattoo = stui::core::TattooEntry{.texturePath = ""}},
            {.index = 4,
                .occupancy = stui::core::SlotOccupancy::slaveTats,
                .tattoo = stui::core::TattooEntry{.texturePath = "owned/b.dds"}},
            {.index = 5, .occupancy = stui::core::SlotOccupancy::empty},
            {.index = 6,
                .occupancy = stui::core::SlotOccupancy::slaveTats,
                .tattoo = stui::core::TattooEntry{.texturePath = "next/c.dds"}},
            {.index = 7,
                .occupancy = stui::core::SlotOccupancy::slaveTats,
                .tattoo = stui::core::TattooEntry{.texturePath = "next/d.dds"}},
        },
    };

    const auto firstPage = stui::native::collectVisibleSlotTexturePaths(slots, 0, 6);
    expect(firstPage == std::vector<std::string>{"owned/a.dds", "owned/b.dds"},
        "expected only owned non-empty paths from the visible slot page");
    const auto secondPage = stui::native::collectVisibleSlotTexturePaths(slots, 1, 6);
    expect(secondPage == std::vector<std::string>{"next/c.dds", "next/d.dds"},
        "expected slot path collection to follow the selected page");
}

void pickerAndPreviewHelpersExposeExactTargetIntent() {
    expect(stui::native::formatSlotTargetLabel(
               stui::core::TattooArea::body, 2) == "Player / BODY / Slot 2",
        "expected exact Player BODY target label");
    expect(stui::native::previewApplyButtonLabel(2) == "Apply to Slot 2",
        "expected Apply action to name the target slot");
    expect(stui::native::previewApplyButtonLabel(2, true) == "Retry Slot 2",
        "expected failed Apply action to identify a retry");
    expect(stui::native::isPreviewApplyEnabled(
               stui::native::SlotWorkflowScreen::preview, true, true),
        "expected complete Preview target to enable Apply");
    expect(!stui::native::isPreviewApplyEnabled(
               stui::native::SlotWorkflowScreen::applying, true, true),
        "expected Applying state to suppress duplicate Apply");
    expect(!stui::native::isPreviewApplyEnabled(
               stui::native::SlotWorkflowScreen::preview, false, true),
        "expected missing slot target to disable Apply");
}

void removeHelpersRequireExplicitTargetConfirmation() {
    expect(stui::native::removeButtonLabel(
               2, stui::native::RemoveButtonState::initial) == "Remove from Slot 2",
        "expected Remove action to name the target slot");
    expect(stui::native::removeButtonLabel(
               2, stui::native::RemoveButtonState::retryRemove) == "Retry Remove Slot 2",
        "expected failed Remove action to identify a retry");
    expect(stui::native::removeButtonLabel(
               2, stui::native::RemoveButtonState::retrySynchronization) ==
               "Retry Sync Slot 2",
        "expected post-mutation failure to retry synchronization only");
    expect(stui::native::isRemoveConfirmationEnabled(
               stui::native::SlotWorkflowScreen::removeConfirmation, true),
        "expected complete Remove confirmation to enable mutation");
    expect(!stui::native::isRemoveConfirmationEnabled(
               stui::native::SlotWorkflowScreen::slotActions, true),
        "expected Slot Actions alone not to enable Remove mutation");
    expect(!stui::native::isRemoveConfirmationEnabled(
               stui::native::SlotWorkflowScreen::removeConfirmation, false),
        "expected missing slot target to disable Remove");
}

void pickerVisiblePathsFollowOnlyTheSixRenderedCards() {
    const stui::repository::TattooPage page{
        .entries = {
            {.texturePath = "picker/a.dds"},
            {.texturePath = "picker/b.dds"},
            {.texturePath = ""},
            {.texturePath = "picker/d.dds"},
            {.texturePath = "picker/e.dds"},
            {.texturePath = "picker/f.dds"},
            {.texturePath = "picker/not-visible.dds"},
        },
    };

    const auto paths = stui::native::collectPickerTexturePaths(page);
    expect(stui::native::pickerVisibleCardCount(page) == 6,
        "expected renderer and thumbnail collector to share a six-card bound");
    expect(paths == std::vector<std::string>{
                        "picker/a.dds",
                        "picker/b.dds",
                        "picker/d.dds",
                        "picker/e.dds",
                        "picker/f.dds"},
        "expected thumbnail work only for non-empty paths in six visible Picker cards");
}

void nullSnapshotModelHasSafeEmptyPageWithoutImGui() {
    stui::native::NativeCatalogBrowserModel model([] { return nullptr; });
    model.refresh();

    const auto& page = model.page();
    expect(page.entries.empty(), "expected no entries for a null snapshot");
    expect(page.totalEntries == 0 && page.matchedEntries == 0,
           "expected an empty catalog page");
    expect(page.pageCount == 0 && page.pageSize == 6,
           "expected safe six-item empty page metadata");

    void (*render)(
        stui::native::NativeSlotWorkflowModel&,
        stui::native::NativeSlotWorkflowRuntime&,
        stui::native::NativeCatalogBrowserModel&,
        stui::native::NativeThumbnailRuntime&,
        const std::function<void()>&) =
        &stui::native::OfficialMenuFrameworkAdapter::renderFoundation;
    (void)render;
}

}  // namespace

int main() {
    try {
        rejectsIncompleteExportTable();
        std::cout << "PASS rejects incomplete export table\n";
        translatesSectionAndNonPausingWindowState();
        std::cout << "PASS translates section and non-pausing window state\n";
        defaultAdapterIsUnavailableWithoutLoadedFramework();
        std::cout << "PASS default adapter is unavailable without loaded framework\n";
        foundationLayoutAnchorsFortyPercentPanelToRightEdge();
        std::cout << "PASS foundation layout anchors panel to right edge\n";
        thumbnailPresentationFitsWithoutStretchingAndLabelsFailures();
        std::cout << "PASS thumbnail presentation fits and labels failures\n";
        thumbnailViewLookupMapsDuplicatePathVariantsToOneView();
        std::cout << "PASS thumbnail view lookup maps duplicate path variants\n";
        thumbnailGridGroupsTwoCardsIntoEachRow();
        std::cout << "PASS thumbnail grid groups two cards into each row\n";
        thumbnailCardWidgetsHaveStableUniqueIds();
        std::cout << "PASS thumbnail card widgets have stable unique IDs\n";
        browserGridUsesRemainingHeightWithoutVerticalScrolling();
        std::cout << "PASS browser grid uses remaining height without scrolling\n";
        catalogBadgeAnchorsInsideThumbnailTopRightCorner();
        std::cout << "PASS catalog badge anchors inside thumbnail top-right\n";
        thumbnailCardsReserveNoPersistentMetadataRow();
        std::cout << "PASS thumbnail cards reserve no persistent metadata row\n";
        footerControlAlignsToRightContentEdge();
        std::cout << "PASS footer control aligns to right content edge\n";
        pickerFooterActionsStayRightAlignedInNavigationOrder();
        std::cout << "PASS Picker footer actions stay right-aligned\n";
        tattooColorComponentsPreserveRgbChannelOrder();
        std::cout << "PASS tattoo color components preserve RGB channel order\n";
        currentSlotColorSwatchUsesOwnedTattooColorAtBottomRight();
        std::cout << "PASS Current Slot color swatch uses owned tattoo color\n";
        currentSlotColorSwatchSkipsEmptyAndExternalSlots();
        std::cout << "PASS Current Slot color swatch skips empty and external slots\n";
        catalogTooltipIncludesInUseSlotDetails();
        std::cout << "PASS catalog tooltip includes In Use Slot details\n";
        pageInputKeepsPendingEditsUntilEnterOrFocusLoss();
        std::cout << "PASS page input keeps pending edits until commit\n";
        classifiesEmptyCatalogSeparatelyFromNoMatches();
        std::cout << "PASS classifies empty catalog separately from no matches\n";
        sourceOptionsDistinguishDuplicatePackNamesAndPreserveIds();
        std::cout << "PASS source options distinguish duplicate pack names\n";
        slotCardsExposeAddReplaceAndDisabledTreatments();
        std::cout << "PASS slot cards expose intended treatments\n";
        slotPaginationClampsToSixCardPages();
        std::cout << "PASS slot pagination clamps to six-card pages\n";
        slotAreaLabelsMatchSlaveTatsAreaNames();
        std::cout << "PASS slot area labels match SlaveTats areas\n";
        visibleSlotPathsIncludeOnlyOwnedCardsOnTheCurrentPage();
        std::cout << "PASS visible slot paths include only owned cards\n";
        pickerAndPreviewHelpersExposeExactTargetIntent();
        std::cout << "PASS picker and Preview helpers expose exact target intent\n";
        removeHelpersRequireExplicitTargetConfirmation();
        std::cout << "PASS Remove helpers require explicit target confirmation\n";
        pickerVisiblePathsFollowOnlyTheSixRenderedCards();
        std::cout << "PASS Picker visible paths follow six rendered cards\n";
        nullSnapshotModelHasSafeEmptyPageWithoutImGui();
        std::cout << "PASS null snapshot model has safe empty page without ImGui\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
