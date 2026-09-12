# PrismaUI Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the SKSE Menu Framework workflow SlaveTatsUI's only frontend and remove every shipped PrismaUI dependency and artifact.

**Architecture:** A new UI-independent `ApplicationRuntime` owns the concrete `SlaveTatsRuntime` and its `SlaveTatsService`. `main.cpp` injects that service into the existing native workflow, forwards external API bindings to the application runtime, and routes the configured hotkey to a tested `NativeMenu::toggle()` path.

**Tech Stack:** C++23, CommonLibSSE-NG, SKSE Menu Framework v3, CMake/CTest, PowerShell/MSVC, nlohmann-json, DirectXTex, D3D11

**Spec:** `docs/superpowers/specs/2026-09-12-prismaui-retirement-design.md`

## Global Constraints

- The native workflow remains player-only.
- Prisma JSON compatibility and old Prisma browser state are not preserved.
- The plugin never removes PrismaUI or legacy cache files from a user's installation.
- Deployment and in-game acceptance require separate approval.
- Historical records under `docs/superpowers` remain unchanged.
- Every production behavior begins with a failing test and is verified in Debug and Release.
- No commit, push, or history rewrite occurs without showing the diff and proposed Conventional Commit message and receiving explicit approval.

---

### Task 1: Native Hotkey Toggle Contract

**Files:**
- Modify: `src/native/NativeMenu.h`
- Modify: `src/native/NativeMenu.cpp`
- Test: `tests/native/NativeMenuTests.cpp`

**Interfaces:**
- Consumes: existing `NativeMenu::open()`, `close()`, `isOpen()`, `isRegistered()`, and `LaunchFunction`.
- Produces: `void NativeMenu::toggle() noexcept`, which closes an open window or runs the launch callback and opens a closed registered window only when launch succeeds.

- [ ] **Step 1: Add failing toggle tests**

Add focused cases using the existing fake `MenuFrameworkPort`:

```cpp
expect(menu.registerMenu(port).has_value(), "registration should succeed");
menu.toggle();
expect(launchCount == 1, "opening toggle should launch the workflow once");
expect(menu.isOpen(), "opening toggle should open the native window");

menu.toggle();
expect(launchCount == 1, "closing toggle should not relaunch the workflow");
expect(!menu.isOpen(), "closing toggle should close the native window");
```

Add a separate case where `LaunchFunction` returns `false`; `toggle()` must leave the window closed. Add a case where launch throws; the window remains closed and `lastError()` becomes `MenuRegistrationError::callbackFailed`.

- [ ] **Step 2: Verify RED**

Run:

```powershell
cmake --build build/debug --target NativeMenuTests
```

Expected: compilation fails because `NativeMenu::toggle()` does not exist.

- [ ] **Step 3: Implement the minimum toggle behavior**

Declare `void toggle() noexcept;` publicly and implement:

```cpp
void NativeMenu::toggle() noexcept {
    if (isOpen()) {
        close();
        return;
    }
    if (!registered_ || !launch_) {
        return;
    }
    try {
        if (launch_()) {
            open();
        }
    } catch (...) {
        lastError_ = MenuRegistrationError::callbackFailed;
    }
}
```

- [ ] **Step 4: Verify GREEN in both configurations**

Run:

```powershell
cmake --build build/debug --target NativeMenuTests
ctest --test-dir build/debug -R '^NativeMenuTests$' --output-on-failure
cmake --build build/release --target NativeMenuTests
ctest --test-dir build/release -R '^NativeMenuTests$' --output-on-failure
```

Expected: both tests pass with zero failures.

- [ ] **Step 5: Review gate**

Show the focused diff and propose, but do not execute without approval:

```text
feat: add native menu hotkey toggle
```

---

### Task 2: UI-Independent Application Runtime

**Files:**
- Create: `src/runtime/ApplicationRuntime.h`
- Create: `src/runtime/ApplicationRuntime.cpp`
- Create: `tests/runtime/ApplicationRuntimeTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `runtime::SlaveTatsRuntime`, `core::SlaveTatsService`, `slavetats::interface::Addresses`, and `jc::root_interface`.
- Produces: `core::SlaveTatsService& service() noexcept`, `void bindSlaveTats(const Addresses*) noexcept`, `void noteSlaveTatsVersionMismatch(std::uint32_t) noexcept`, and `bool bindJContainers(const jc::root_interface*)`.

- [ ] **Step 1: Add a failing application runtime test target**

Create a test which constructs `ApplicationRuntime`, records a version mismatch, and verifies the concrete service observes an unavailable API while `apiVersion()` exposes the received version for diagnostics:

```cpp
stui::runtime::ApplicationRuntime application;
application.noteSlaveTatsVersionMismatch(99);

expect(!application.runtime().apiAvailable(), "mismatched API must remain unavailable");
expect(application.runtime().apiVersion() == 99, "diagnostic API version must be retained");
expect(&application.service() == &application.service(), "application must expose one stable service");
```

Add a second case with a zero-initialized `Addresses` value and verify `bindSlaveTats(&api)` makes `apiAvailable()` true and preserves the pointer. The test exercises the real concrete runtime and service; it does not mock forwarding calls.

Add `ApplicationRuntimeTests` to CMake with `ApplicationRuntime.cpp`, `SlaveTatsRuntime.cpp`, `SlaveTatsService.cpp`, `OverlaySlotConfiguration.cpp`, and `UpdateTattooAppearanceOrchestration.cpp`, matching the existing runtime test linkage to CommonLibSSE.

- [ ] **Step 2: Verify RED**

Run:

```powershell
cmake --build build/debug --target ApplicationRuntimeTests
```

Expected: configure/build fails because `ApplicationRuntime` files do not exist.

- [ ] **Step 3: Implement the runtime owner**

Implement the exact ownership shape:

```cpp
class ApplicationRuntime {
public:
    void bindSlaveTats(const slavetats::interface::Addresses* api) noexcept;
    void noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept;
    [[nodiscard]] bool bindJContainers(const jc::root_interface* root);
    [[nodiscard]] core::SlaveTatsService& service() noexcept;
    [[nodiscard]] SlaveTatsRuntime& runtime() noexcept;

private:
    SlaveTatsRuntime runtime_;
    core::SlaveTatsService service_{runtime_};
};
```

Each method delegates to or returns the owned concrete object. Do not add UI, logging, singleton, JSON, or texture responsibilities.

- [ ] **Step 4: Verify GREEN in both configurations**

Run the target and its CTest entry for `build/debug` and `build/release`. Expected: both pass with zero failures.

- [ ] **Step 5: Review gate**

Show the focused diff and propose, but do not execute without approval:

```text
refactor: add UI-independent application runtime
```

---

### Task 3: Switch Plugin Composition to Native-Only

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Delete: `include/PrismaUI_API.h`
- Delete: `src/Bridge.h`
- Delete: `src/Bridge.cpp`
- Delete: `src/adapters/PrismaSlotSerializer.h`
- Delete: `src/adapters/PrismaSlotSerializer.cpp`
- Delete: `src/adapters/PrismaTattooSerializer.h`
- Delete: `src/adapters/PrismaTattooSerializer.cpp`
- Delete: `tests/adapters/PrismaSlotSerializerTests.cpp`
- Delete: `tests/adapters/PrismaTattooSerializerTests.cpp`
- Delete: `view/index.html`

**Interfaces:**
- Consumes: Task 1 `NativeMenu::toggle()` and Task 2 `ApplicationRuntime::service()` plus binding methods.
- Produces: a native-only plugin binary whose hotkey and Menu Framework section item launch the same player workflow.

- [ ] **Step 1: Establish the integration failure before removing Bridge**

Replace only the `Bridge` service dependency in `main.cpp` with a declared `runtime::ApplicationRuntime g_applicationRuntime` and inject `g_applicationRuntime.service()` into the four native workflow callbacks. Do not yet create its files in the target source list.

- [ ] **Step 2: Verify RED**

Run:

```powershell
cmake --build build/debug --target SlaveTatsUI
```

Expected: link failure for missing `ApplicationRuntime` definitions or build failure because the new implementation is not in the plugin target.

- [ ] **Step 3: Complete native-only composition**

Add `ApplicationRuntime.h/.cpp` to the plugin target. In `main.cpp`:

- replace every `Bridge::get()->tattooService()` with `g_applicationRuntime.service()`;
- forward SlaveTatsNG and JContainers messages to `g_applicationRuntime`;
- make the version-mismatch handler call `noteSlaveTatsVersionMismatch`;
- remove `Bridge::onDataLoaded()`;
- give the process-lifetime native menu a scope accessible to `InputSink`;
- call `g_nativeMenu.toggle()` for the configured key;
- log Native Menu registration failure without a Prisma fallback claim.

Remove Prisma/Bridge/serializer files and targets from CMake, delete their source and test files, delete `view/index.html`, and change the project description to `Native SlaveTats UI plugin`.

- [ ] **Step 4: Verify the focused integration**

Run:

```powershell
cmake --build build/debug --target SlaveTatsUI NativeMenuTests ApplicationRuntimeTests
ctest --test-dir build/debug -R '^(NativeMenuTests|ApplicationRuntimeTests|NativeSlotWorkflowModelTests|NativeSlotWorkflowRuntimeTests|OfficialMenuFrameworkAdapterTests)$' --output-on-failure
```

Expected: the plugin links and every focused native/runtime test passes.

- [ ] **Step 5: Verify retirement boundaries**

Run:

```powershell
rg -n -i 'PrismaUI_API|PrismaView|IVPrismaUI|InteropCall|RegisterJSListener|base64|\.rgba' CMakeLists.txt src include tests build.ps1 README.md DEVELOPMENT.md DEPLOY.md
```

Expected at this stage: only documentation/build-script matches remain; no production, include, CMake, or test match remains.

- [ ] **Step 6: Review gate**

Show the focused diff and propose, but do not execute without approval:

```text
refactor: retire PrismaUI runtime
```

---

### Task 4: Native-Only Packaging, Documentation, and Full Verification

**Files:**
- Modify: `build.ps1`
- Modify: `README.md`
- Modify: `DEVELOPMENT.md`
- Modify: `DEPLOY.md`
- Modify: `SKSE_MENU_MIGRATION.md`
- Modify: `docs/superpowers/specs/2026-09-12-prismaui-retirement-design.md`
- Add: `docs/superpowers/plans/2026-09-12-prismaui-retirement.md`

**Interfaces:**
- Consumes: the native-only binary and file layout from Task 3.
- Produces: accurate build, installation, deployment, troubleshooting, architecture, and completed-migration guidance.

- [ ] **Step 1: Update build and deployment contract**

Remove the `view/index.html -> PrismaUI/views/...` build message. Document a DLL-only mod layout under `SKSE/Plugins`, require SKSE Menu Framework v3, and remove all instructions to copy browser assets.

- [ ] **Step 2: Update user and developer documentation**

Describe the native slot-first player workflow, GPU-resident current-page thumbnails, hotkey toggle, Menu Framework section item, and native error diagnosis. Remove current claims about actor switching, browser overlay dimensions, persistent thumbnail cache, Prisma requirements, and Prisma troubleshooting. Mark the migration document as completed and link this retirement decision; retain its historical phase text.

- [ ] **Step 3: Run documentation/source hygiene checks**

Run:

```powershell
rg -n -i 'PrismaUI_API|PrismaView|IVPrismaUI|InteropCall|RegisterJSListener|base64|\.rgba|PrismaUI\\views' CMakeLists.txt src include tests build.ps1 README.md DEVELOPMENT.md DEPLOY.md
git diff --check
```

Expected: no search matches and `git diff --check` exits zero. A separate search may still find Prisma references in historical `docs/superpowers` records and the migration history.

- [ ] **Step 4: Build and test Debug from a configured MSVC environment**

Run:

```powershell
.\build.ps1 -Config debug
ctest --test-dir build/debug --output-on-failure
```

Expected: plugin build succeeds and 100% of Debug tests pass.

- [ ] **Step 5: Build and test Release from a configured MSVC environment**

Run:

```powershell
.\build.ps1 -Config release
ctest --test-dir build/release --output-on-failure
```

Expected: plugin build succeeds and 100% of Release tests pass.

- [ ] **Step 6: Audit the final change**

Run `git status --short`, `git diff --stat`, `git diff --check`, and inspect the complete diff for accidental secrets, unrelated edits, stale file paths, and violation of the design spec. Do not claim in-game verification unless an approved deployment and manual acceptance run occurred.

- [ ] **Step 7: Final commit approval gate**

Present the complete diff summary, all verification evidence, known limitation that in-game acceptance is pending, and this proposed squashed Conventional Commit message:

```text
refactor: retire PrismaUI frontend
```

Wait for explicit approval before any `git commit`. Request deployment approval separately after commit handling.
