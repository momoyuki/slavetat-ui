#include "native/OfficialMenuFrameworkAdapter.h"
#include "native/NativeCatalogBrowserModel.h"

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

void nullSnapshotModelHasSafeEmptyPageWithoutImGui() {
    stui::native::NativeCatalogBrowserModel model([] { return nullptr; });
    model.refresh();

    const auto& page = model.page();
    expect(page.entries.empty(), "expected no entries for a null snapshot");
    expect(page.totalEntries == 0 && page.matchedEntries == 0,
           "expected an empty catalog page");
    expect(page.pageCount == 0 && page.pageSize == 6,
           "expected safe six-item empty page metadata");

    void (*render)(stui::native::NativeCatalogBrowserModel&, const std::function<void()>&) =
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
        pageInputKeepsPendingEditsUntilEnterOrFocusLoss();
        std::cout << "PASS page input keeps pending edits until commit\n";
        classifiesEmptyCatalogSeparatelyFromNoMatches();
        std::cout << "PASS classifies empty catalog separately from no matches\n";
        sourceOptionsDistinguishDuplicatePackNamesAndPreserveIds();
        std::cout << "PASS source options distinguish duplicate pack names\n";
        nullSnapshotModelHasSafeEmptyPageWithoutImGui();
        std::cout << "PASS null snapshot model has safe empty page without ImGui\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
