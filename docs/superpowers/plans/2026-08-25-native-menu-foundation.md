# Native Menu Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an optional, non-pausing SlaveTatsUI foundation window to SKSE Menu Framework 3.x while preserving the working F8 PrismaUI frontend.

**Architecture:** A framework-independent `NativeMenu` owns registration state and callback routing through a narrow `MenuFrameworkPort`. `OfficialMenuFrameworkAdapter` is the only production module that knows the vendored SKSE Menu Framework ABI and ImGuiMCP, while `main.cpp` remains the composition root.

**Tech Stack:** C++23, CommonLibSSE-NG, SKSE Menu Framework v3 dynamic exports, ImGuiMCP, CMake/CTest, MSVC, PowerShell

**Spec:** `docs/superpowers/specs/2026-08-25-native-menu-foundation-design.md`

## Global Constraints

- Target SKSE Menu Framework 3.x; treat a missing DLL, missing export, or another major version as an optional-feature failure.
- Vendor the official v3 client header from revision `3a65dc0147388da177c324cff4d89d9e25094623` with upstream attribution and LGPL-2.1 notice intact.
- Keep the existing F8 PrismaUI workflow unchanged.
- Register `SlaveTatsUI/Tattoo Browser` and one non-pausing window only once.
- Keep SKSE Menu Framework and ImGuiMCP types out of core, repository, runtime, texture, and `NativeMenu` public interfaces.
- Do not perform repository, filesystem, DDS, D3D11, SlaveTatsNG, or JContainers work from the first native render callback.
- Use `build.ps1` so MSVC and Windows SDK headers are available.
- Do not commit any task until its diff, checks, and proposed Conventional Commit message have been shown to the user and explicitly approved.

## File Structure

- Create `src/native/MenuFrameworkPort.h`: application-owned framework boundary, registration result types, callback aliases, and opaque window token.
- Create `src/native/NativeMenu.h`: framework-independent registration and presentation-state API.
- Create `src/native/NativeMenu.cpp`: idempotent registration, callback routing, exception containment, and safe open/close behavior.
- Create `tests/native/NativeMenuTests.cpp`: fake-port unit tests for all registration and callback behavior.
- Create `include/SKSEMenuFramework.h`: pinned upstream dynamic client API and ImGuiMCP declarations.
- Create `src/native/OfficialMenuFrameworkAdapter.h`: production adapter API with no official types exposed to callers.
- Create `src/native/OfficialMenuFrameworkAdapter.cpp`: export validation, version gate, native rendering, and `WindowInterface` translation.
- Create `tests/native/OfficialMenuFrameworkAdapterTests.cpp`: tests against injected export bindings without loading Skyrim or the framework DLL.
- Modify `src/main.cpp`: create process-lifetime native objects and attempt optional registration during plugin load.
- Modify `CMakeLists.txt`: compile native sources/header and register both native test executables.

---

### Task 1: Framework-Independent Native Menu Registration

**Files:**
- Create: `src/native/MenuFrameworkPort.h`
- Create: `src/native/NativeMenu.h`
- Create: `src/native/NativeMenu.cpp`
- Create: `tests/native/NativeMenuTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class MenuRegistrationError { unavailable, unsupportedVersion, missingExport, windowCreationFailed, callbackFailed }`.
- Produces: `using MenuCallback = void (*)() noexcept` and opaque `using MenuWindow = std::uintptr_t`.
- Produces: `MenuFrameworkPort::version()`, `setSection()`, `addSectionItem()`, `addWindow()`, `setWindowOpen()`, and `isWindowOpen()`.
- Produces: `NativeMenu::registerMenu(MenuFrameworkPort&)`, `NativeMenu::open()`, `NativeMenu::close()`, `NativeMenu::isRegistered()`, `NativeMenu::lastError()`, and ABI-safe static callbacks.

- [ ] **Step 1: Add the failing registration tests**

Create `tests/native/NativeMenuTests.cpp` with a `FakeMenuFrameworkPort` that records section names, item paths, callbacks, pause flags, window tokens, and open-state writes. Cover these exact cases:

```cpp
void registersOneNonPausingTattooBrowser() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;

    const auto result = menu.registerMenu(port);

    expect(result.has_value(), "expected native menu registration");
    expect(port.section == "SlaveTatsUI", "expected owning section");
    expect(port.itemPath == "Tattoo Browser", "expected browser item");
    expect(port.windowRegistrations == 1, "expected one native window");
    expect(!port.pauseGame, "expected non-pausing window");
}

void repeatedRegistrationIsIdempotent() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;
    expect(menu.registerMenu(port).has_value(), "expected initial registration");
    expect(menu.registerMenu(port).has_value(), "expected repeated success");
    expect(port.itemRegistrations == 1 && port.windowRegistrations == 1,
           "expected no duplicate registrations");
}

void sectionCallbackOpensWindow() {
    FakeMenuFrameworkPort port;
    stui::native::NativeMenu menu;
    expect(menu.registerMenu(port).has_value(), "expected registration");
    port.itemCallback();
    expect(port.open, "expected section callback to open window");
}
```

Also test unavailable ports, version `2.9F`, missing item/window operations, a zero window token, a thrown render delegate, close/reopen, and destruction clearing the callback target.

- [ ] **Step 2: Register the test target and verify it fails**

Add this target inside `if(BUILD_TESTING)`:

```cmake
add_executable(NativeMenuTests
    src/native/NativeMenu.cpp
    tests/native/NativeMenuTests.cpp
)
target_include_directories(NativeMenuTests PRIVATE src)
add_test(NAME NativeMenuTests COMMAND NativeMenuTests)
```

Run:

```powershell
./build.ps1 -Config debug
```

Expected: compilation fails because `native/NativeMenu.h` and `native/MenuFrameworkPort.h` do not exist.

- [ ] **Step 3: Define the narrow framework port**

Create `src/native/MenuFrameworkPort.h` with this application-owned interface:

```cpp
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
```

- [ ] **Step 4: Implement minimal idempotent registration and callback containment**

Create `NativeMenu.h/.cpp`. Store the port and opaque window only after every registration operation succeeds. Use one process-lifetime active callback target and clear it in the destructor. `registerMenu()` must:

```cpp
if (registered_) return {};
if (!port.available()) return fail(MenuRegistrationError::unavailable);
if (const auto version = port.version(); version < 3.0F || version >= 4.0F) {
    return fail(MenuRegistrationError::unsupportedVersion);
}
if (auto result = port.setSection("SlaveTatsUI"); !result) return fail(result.error());
if (auto result = port.addSectionItem("Tattoo Browser", &NativeMenu::sectionCallback);
    !result) return fail(result.error());
auto window = port.addWindow(&NativeMenu::renderCallback, false);
if (!window || *window == 0) return fail(MenuRegistrationError::windowCreationFailed);
```

The static callbacks must be declared `noexcept`, check that an active target exists, and catch all exceptions before returning across the ABI. The foundation render delegate is injected as `std::function<void()>`; its default is empty so Task 1 stays independent of ImGui.

- [ ] **Step 5: Run the focused and complete Debug suites**

Run:

```powershell
./build.ps1 -Config debug
ctest --test-dir build/debug -C Debug --output-on-failure -R NativeMenuTests
ctest --test-dir build/debug -C Debug --output-on-failure
```

Expected: `NativeMenuTests` passes and the complete Debug CTest suite passes.

- [ ] **Step 6: Review gate for the first deliverable**

Run `git diff --check`, scan the Task 1 diff for secrets, machine paths, debug markers, and unintended Prisma changes, then show the diff and propose:

```text
feat: add native menu registration core
```

Wait for explicit approval before committing.

---

### Task 2: Official SKSE Menu Framework Adapter and Foundation Renderer

**Files:**
- Create: `include/SKSEMenuFramework.h`
- Create: `src/native/OfficialMenuFrameworkAdapter.h`
- Create: `src/native/OfficialMenuFrameworkAdapter.cpp`
- Create: `tests/native/OfficialMenuFrameworkAdapterTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MenuFrameworkPort`, `MenuCallback`, `MenuWindow`, and `RegistrationResult` from Task 1.
- Produces: `MenuFrameworkBindings`, an injectable table for `GetMenuFrameworkVersion`, `AddSectionItem`, and `AddWindow` exports.
- Produces: `OfficialMenuFrameworkAdapter final : public MenuFrameworkPort`.
- Produces: `OfficialMenuFrameworkAdapter::renderFoundation() noexcept`, which draws only static status text in a right-side window.

- [ ] **Step 1: Vendor and verify the pinned official client header**

Copy the official v3 client header from repository revision `3a65dc0147388da177c324cff4d89d9e25094623` into `include/SKSEMenuFramework.h`. Preserve the upstream license notice and add a short provenance comment containing the repository URL and exact revision. Do not modify exported function names or ImGuiMCP declarations.

Verify:

```powershell
git diff -- include/SKSEMenuFramework.h
rg -n "3a65dc0147388da177c324cff4d89d9e25094623|LGPL|GetMenuFrameworkVersion|AddSectionItem|AddWindow" include/SKSEMenuFramework.h
```

Expected: all provenance and required API markers are present.

- [ ] **Step 2: Add failing adapter tests with injected exports**

Create a bindings seam in `OfficialMenuFrameworkAdapter.h`:

```cpp
struct MenuFrameworkBindings {
    using VersionFn = float (*)();
    using AddSectionItemFn = void (*)(const char*, MenuCallback);
    using AddWindowFn = void* (*)(MenuCallback);

    VersionFn getVersion{};
    AddSectionItemFn addSectionItem{};
    AddWindowFn addWindow{};
};
```

In `OfficialMenuFrameworkAdapterTests.cpp`, provide fake functions and cover:

```cpp
void rejectsMissingRequiredExport() {
    stui::native::MenuFrameworkBindings bindings{};
    bindings.getVersion = &returnVersionThree;
    stui::native::OfficialMenuFrameworkAdapter adapter(bindings);
    expect(!adapter.available(), "expected missing export rejection");
}

void translatesWindowStateWithoutExposingOfficialType() {
    FakeWindow window;
    auto bindings = completeBindingsReturning(window);
    stui::native::OfficialMenuFrameworkAdapter adapter(bindings);
    auto token = adapter.addWindow(&emptyCallback, false);
    expect(token.has_value(), "expected opaque window token");
    adapter.setWindowOpen(*token, true);
    expect(window.IsOpen, "expected official window to open");
    expect(!window.BlockUserInput, "expected non-pausing window");
}
```

Also test absent DLL bindings, versions below 3 and at least 4, null window return, section-prefix composition, and callback pointer forwarding.

- [ ] **Step 3: Register the adapter test and verify it fails**

Add:

```cmake
add_executable(OfficialMenuFrameworkAdapterTests
    src/native/OfficialMenuFrameworkAdapter.cpp
    tests/native/OfficialMenuFrameworkAdapterTests.cpp
)
target_include_directories(OfficialMenuFrameworkAdapterTests PRIVATE src include)
add_test(NAME OfficialMenuFrameworkAdapterTests COMMAND OfficialMenuFrameworkAdapterTests)
```

Run `./build.ps1 -Config debug`.

Expected: compilation fails because the production adapter is not implemented.

- [ ] **Step 4: Implement export resolution and safe translation**

The default constructor resolves `SKSEMenuFramework.dll` with `GetModuleHandleW` and obtains all three required exports with `GetProcAddress`. The injected constructor accepts `MenuFrameworkBindings` for tests. Never use `std::filesystem::exists()` as the availability decision; the loaded module and complete export table are authoritative.

`setSection()` stores the section string inside the adapter. `addSectionItem()` passes `"SlaveTatsUI/Tattoo Browser"` to the raw export so failure can be determined before partial registration. `addWindow()` converts the returned `WindowInterface*` to `MenuWindow`, sets `BlockUserInput` from `pauseGame`, and rejects null.

- [ ] **Step 5: Implement the static foundation renderer**

Use only ImGuiMCP declarations from the vendored header. On first appearance, anchor the window to the right with 20 px margins and 40 percent of viewport width:

```cpp
const auto* viewport = ImGuiMCP::GetMainViewport();
const ImGuiMCP::ImVec2 size{viewport->Size.x * 0.4F, viewport->Size.y - 40.0F};
const ImGuiMCP::ImVec2 position{
    viewport->Pos.x + viewport->Size.x - size.x - 20.0F,
    viewport->Pos.y + 20.0F,
};
ImGuiMCP::SetNextWindowPos(position, ImGuiMCP::ImGuiCond_Appearing, {0.0F, 0.0F});
ImGuiMCP::SetNextWindowSize(size, ImGuiMCP::ImGuiCond_Appearing);
ImGuiMCP::Begin("Tattoo Browser##SlaveTatsUI", nullptr,
                ImGuiMCP::ImGuiWindowFlags_NoCollapse);
ImGuiMCP::TextUnformatted("Native menu foundation is ready.");
ImGuiMCP::TextUnformatted("Catalog browsing remains available through PrismaUI (F8).");
ImGuiMCP::End();
```

Keep all ImGuiMCP calls in `OfficialMenuFrameworkAdapter.cpp`. Catch exceptions in the `NativeMenu` callback boundary established in Task 1.

- [ ] **Step 6: Run focused, Debug, and Release verification**

Run:

```powershell
./build.ps1 -Config debug
ctest --test-dir build/debug -C Debug --output-on-failure -R "NativeMenuTests|OfficialMenuFrameworkAdapterTests"
ctest --test-dir build/debug -C Debug --output-on-failure
./build.ps1 -Config release
ctest --test-dir build/release -C Release --output-on-failure
```

Expected: both native test targets and every existing test pass in both configurations.

- [ ] **Step 7: Review gate for the adapter deliverable**

Run `git diff --check`, confirm no official/ImGui types appear outside `include/SKSEMenuFramework.h` and `src/native/OfficialMenuFrameworkAdapter.cpp`, scan for secrets and machine-specific paths, then show the diff and propose:

```text
feat: add SKSE menu framework adapter
```

Wait for explicit approval before committing.

---

### Task 3: Plugin Lifecycle Integration and Runtime Diagnostics

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/native/NativeMenu.h`
- Modify: `src/native/NativeMenu.cpp`
- Test: `tests/native/NativeMenuTests.cpp`

**Interfaces:**
- Consumes: `NativeMenu::registerMenu()` and `OfficialMenuFrameworkAdapter` from Tasks 1 and 2.
- Produces: process-lifetime `g_menuFrameworkAdapter` and `g_nativeMenu` composition-root objects.
- Produces: one named log outcome for registered, unavailable, unsupported-version, missing-export, and window-creation failure states.

- [ ] **Step 1: Add failing registration-status diagnostics tests**

Extend `NativeMenuTests.cpp` to verify `lastError()` is empty after success and contains the exact failure enum after each failure. Verify a failed first attempt may be retried after the fake port becomes available, without preserving a partial registered state.

```cpp
void unavailableRegistrationCanBeRetried() {
    FakeMenuFrameworkPort port;
    port.isAvailable = false;
    stui::native::NativeMenu menu;
    expect(!menu.registerMenu(port), "expected unavailable result");
    expect(menu.lastError() == stui::native::MenuRegistrationError::unavailable,
           "expected named unavailable state");
    port.isAvailable = true;
    expect(menu.registerMenu(port).has_value(), "expected retry to succeed");
}
```

Run the focused test and expect failure until retry state and `lastError()` behavior are complete.

- [ ] **Step 2: Add process-lifetime native composition in `main.cpp`**

Beside `g_tattooCatalogStore`, construct:

```cpp
native::OfficialMenuFrameworkAdapter g_menuFrameworkAdapter;
native::NativeMenu g_nativeMenu(&native::OfficialMenuFrameworkAdapter::renderFoundation);
```

After logging is initialized in `SKSEPluginLoad`, call `g_nativeMenu.registerMenu(g_menuFrameworkAdapter)`. Convert the error enum to a stable English log message. Failures log one warning and continue plugin initialization; success logs framework version and `non-pausing=true`. Do not place this call in `kDataLoaded`, and do not change `InputSink`, `Config::hotkeyDIK`, or `Bridge::toggleUI()`.

- [ ] **Step 3: Add all native files to the plugin target**

Update `headers` and `sources` in `CMakeLists.txt` with:

```cmake
include/SKSEMenuFramework.h
src/native/MenuFrameworkPort.h
src/native/NativeMenu.h
src/native/OfficialMenuFrameworkAdapter.h
src/native/NativeMenu.cpp
src/native/OfficialMenuFrameworkAdapter.cpp
```

Do not add a link dependency on `SKSEMenuFramework.dll`; runtime export resolution must remain dynamic.

- [ ] **Step 4: Run full automated verification and hygiene checks**

Run:

```powershell
./build.ps1 -Config debug
ctest --test-dir build/debug -C Debug --output-on-failure
./build.ps1 -Config release
ctest --test-dir build/release -C Release --output-on-failure
git diff --check
rg -n "TODO|FIXME|PLACEHOLDER|D:\\\\|C:\\\\Users|password|secret|token" src include tests CMakeLists.txt
```

Expected: both builds and all tests pass; whitespace, placeholder, machine-path, and secret scans return no introduced findings. Confirm `rg -n "toggleUI|hotkeyDIK" src/main.cpp` shows the existing F8 path unchanged.

- [ ] **Step 5: Perform the in-game smoke test**

Deploy only through the existing MO2 workflow, then:

1. Open F1 and locate `SlaveTatsUI/Tattoo Browser`.
2. Open the right-side panel; verify the actor remains visible and the game does not pause.
3. Close and reopen it at least three times; verify no crash or duplicate entry.
4. Press F8 and verify PrismaUI still opens.
5. Browse and apply/remove one known tattoo in PrismaUI.
6. Inspect `SlaveTatsUI.log` for one successful native registration and no missing-export or callback errors.
7. Disable SKSE Menu Framework in MO2 once, launch, and verify SlaveTatsUI plus PrismaUI still load with one optional-dependency warning.

- [ ] **Step 6: Review gate for lifecycle integration**

Show the complete Task 3 diff, automated results, and smoke-test log evidence. Propose:

```text
feat: register native tattoo browser foundation
```

Wait for explicit approval before committing.

---

## Completion Check

- The F1 framework contains exactly one `SlaveTatsUI/Tattoo Browser` entry.
- Its right-side window is non-pausing, movable after first appearance, and contains only foundation status content.
- Missing/incompatible SKSE Menu Framework installations do not block plugin or PrismaUI startup.
- F8 still invokes `Bridge::toggleUI()` and the full Prisma workflow remains operational.
- No repository, texture, D3D11, SlaveTatsNG, or JContainers calls occur in the native render callback.
- Debug and Release builds, the full CTest suites, hygiene scans, and the in-game checklist pass.
