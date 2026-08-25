#include "native/OfficialMenuFrameworkAdapter.h"

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
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
