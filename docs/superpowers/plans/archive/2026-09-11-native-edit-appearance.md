# Native Edit Appearance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a native Edit Appearance workflow that safely changes a SlaveTats-managed Current Slot's color and alpha through the same typed service used by PrismaUI.

**Architecture:** Add a typed appearance-update request to Core, validate it in `SlaveTatsService`, and implement handle-membership validation plus JContainers mutation in `SlaveTatsRuntime`. Extend the native workflow model/runtime with an explicit edit session and synchronization-only retry, reuse the Preview appearance controls in the Menu Framework adapter, and route Prisma's existing `updateTattoo` handler through the shared service without changing its JSON contract.

**Tech Stack:** C++23, CommonLibSSE NG, SlaveTatsNG API, JContainers, SKSE task interface, ImGui Menu Framework, CMake/Ninja, CTest, PowerShell.

**Spec:** `docs/superpowers/specs/2026-09-11-native-edit-appearance-design.md`

## Global Constraints

- Native Edit Appearance targets the Player only; the shared service continues to accept an actor form ID for Prisma compatibility.
- Treat every runtime handle as session-local and revalidate it against the requested actor's current applied tattoos before mutation.
- Store visible alpha in Core and Native; convert only at the runtime boundary with `toSlaveTatsInvertedAlpha()`.
- Never call `applyToSlot` to edit appearance.
- Never add JContainers or SlaveTatsNG logic to Native model, coordinator, or adapter code.
- Preserve Prisma request fields and the exact success response `{"type":"success","action":"updateTattoo"}`.
- A synchronization failure after mutation is partial success; retry synchronizes only and never rewrites appearance.
- Use Red-Green-Refactor and run the focused test target after every implementation step.
- Before every commit, stage only scoped files, show the staged diff and exact Conventional Commit message, and wait for explicit user approval.
- Do not deploy a DLL without separate explicit Deploy approval.

---

## File Structure

- `src/core/TattooModels.h`: transport-independent update mode, request, success, result, and stable error codes.
- `src/core/ITattooRuntime.h`: runtime port for appearance updates.
- `src/core/SlaveTatsService.h/.cpp`: dependency and value validation before forwarding.
- `src/runtime/SlaveTatsRuntime.h/.cpp`: actor resolution, applied-handle membership check, field writes, updated flag, and synchronization.
- `src/adapters/PrismaTattooSerializer.h/.cpp`: exact Prisma update success serialization.
- `src/Bridge.cpp`: compatibility adapter from the existing Prisma command to the shared service.
- `src/native/NativeSlotWorkflowModel.h/.cpp`: edit-session state, dirty detection, tickets, transitions, and area refresh.
- `src/native/NativeSlotWorkflowRuntime.h/.cpp`: game-thread scheduling for typed appearance tickets.
- `src/native/OfficialMenuFrameworkAdapter.h/.cpp`: reusable appearance helpers and Edit Appearance rendering.
- Existing tests under `tests/core`, `tests/adapters`, and `tests/native`: seam-level regression coverage without introducing a new test framework.

### Task 1: Add the Typed Core Appearance Contract

**Files:**
- Modify: `src/core/TattooModels.h`
- Modify: `src/core/ITattooRuntime.h`
- Modify: `src/core/SlaveTatsService.h`
- Modify: `src/core/SlaveTatsService.cpp`
- Test: `tests/core/SlaveTatsServiceTests.cpp`

**Interfaces:**
- Produces: `UpdateTattooAppearanceMode`, `UpdateTattooAppearanceRequest`, `UpdateTattooAppearanceSuccess`, `UpdateTattooAppearanceResult`.
- Produces: `ITattooRuntime::updateAppearance(const UpdateTattooAppearanceRequest&)`.
- Produces: `SlaveTatsService::updateAppearance(const UpdateTattooAppearanceRequest&)`.
- Produces: `ServiceErrorCode::staleTattooHandle` and `ServiceErrorCode::updateFailed`.

- [ ] **Step 1: Extend the fake runtime and write failing service tests**

Add a fake implementation that records the request:

```cpp
UpdateTattooAppearanceResult updateAppearance(
    const UpdateTattooAppearanceRequest& request) override {
    ++updateCount;
    updatedRequest = request;
    return UpdateTattooAppearanceSuccess{
        .actorFormId = request.actorFormId,
        .runtimeHandle = request.runtimeHandle,
    };
}
```

Add focused cases for unavailable APIs, zero actor, zero handle in full-update mode, color below `0`, color above `0xFFFFFF`, alpha below `0`, above `1`, and NaN. Add boundary cases for black/white and alpha `0`/`1`, plus a synchronization-only request that is forwarded unchanged.

- [ ] **Step 2: Run the Core test target and verify Red**

Run:

```powershell
cmake --build build/debug --target SlaveTatsUICoreTests
ctest --test-dir build/debug -R '^SlaveTatsUICoreTests$' --output-on-failure
```

Expected: compilation fails because the appearance types and virtual operation do not exist.

- [ ] **Step 3: Add the minimum Core models and port**

Add:

```cpp
enum class UpdateTattooAppearanceMode {
    updateAndSynchronize,
    synchronizeOnly,
};

struct UpdateTattooAppearanceRequest {
    std::uint32_t actorFormId{};
    std::int32_t runtimeHandle{};
    std::int32_t color{0xFFFFFF};
    float alpha{1.0F};
    UpdateTattooAppearanceMode mode{
        UpdateTattooAppearanceMode::updateAndSynchronize};
};

struct UpdateTattooAppearanceSuccess {
    std::uint32_t actorFormId{};
    std::int32_t runtimeHandle{};
};

using UpdateTattooAppearanceResult =
    std::expected<UpdateTattooAppearanceSuccess, ServiceError>;
```

Add the matching pure virtual method, service declaration, error codes, and service validation. Use the existing NaN-safe range form:

```cpp
if (!(request.alpha >= 0.0F && request.alpha <= 1.0F)) {
    return std::unexpected(ServiceError{
        ServiceErrorCode::updateFailed,
        "Tattoo alpha must be between 0 and 1",
    });
}
```

Reject an invalid handle only for `updateAndSynchronize`; synchronization-only retry needs actor identity but performs no handle dereference.

- [ ] **Step 4: Run the focused Core tests and verify Green**

Run the commands from Step 2.

Expected: build succeeds and `SlaveTatsUICoreTests` passes.

- [ ] **Step 5: Prepare the Core commit gate**

Run `git diff --check`, stage only the five Task 1 files, inspect `git diff --cached`, scan the staged diff for secrets, and propose:

```text
feat: add tattoo appearance update contract
```

Wait for explicit approval before running `git commit`.

### Task 2: Implement Safe Runtime Mutation and Preserve Prisma Contract

**Files:**
- Modify: `src/runtime/SlaveTatsRuntime.h`
- Modify: `src/runtime/SlaveTatsRuntime.cpp`
- Modify: `src/adapters/PrismaTattooSerializer.h`
- Modify: `src/adapters/PrismaTattooSerializer.cpp`
- Modify: `src/Bridge.cpp`
- Test: `tests/adapters/PrismaTattooSerializerTests.cpp`
- Test: `tests/runtime/SlaveTatsAlphaTests.cpp`

**Interfaces:**
- Consumes: `UpdateTattooAppearanceRequest` and `UpdateTattooAppearanceResult` from Task 1.
- Produces: `SlaveTatsRuntime::updateAppearance(const core::UpdateTattooAppearanceRequest&)`.
- Produces: `adapters::toPrismaUpdateTattooSuccessJSON()` returning the exact legacy response.

- [ ] **Step 1: Write failing compatibility and conversion tests**

Extend `PrismaTattooSerializerTests.cpp` with:

```cpp
const auto updateSuccess = stui::adapters::toPrismaUpdateTattooSuccessJSON();
if (updateSuccess != R"({"type":"success","action":"updateTattoo"})") {
    return 1;
}
```

Keep `SlaveTatsAlphaTests` as the executable specification for visible-to-inverted alpha, adding an explicit representative assertion if `0.35F -> 0.65F` is not already covered.

- [ ] **Step 2: Run focused tests and verify Red**

Run:

```powershell
cmake --build build/debug --target PrismaTattooSerializerTests SlaveTatsAlphaTests
ctest --test-dir build/debug -R '^(PrismaTattooSerializerTests|SlaveTatsAlphaTests)$' --output-on-failure
```

Expected: Prisma serializer compilation fails because the new helper is absent.

- [ ] **Step 3: Implement the Prisma serializer helper**

Add:

```cpp
std::string toPrismaUpdateTattooSuccessJSON() {
    return R"({"type":"success","action":"updateTattoo"})";
}
```

Run the focused tests and confirm they pass before touching runtime code.

- [ ] **Step 4: Implement runtime handle validation and mutation**

Declare the override in `SlaveTatsRuntime.h`. In `SlaveTatsRuntime.cpp`, implement these exact branches:

```cpp
if (request.mode == core::UpdateTattooAppearanceMode::updateAndSynchronize) {
    // Query this actor's applied tattoos into a scoped JContainer pool.
    // Walk the returned handles and require candidate == request.runtimeHandle.
    // Return staleTattooHandle before writes when no exact match exists.
    jcmini::JMap::setInt(request.runtimeHandle, "color", request.color);
    jcmini::JMap::setFlt(
        request.runtimeHandle,
        "invertedAlpha",
        toSlaveTatsInvertedAlpha(request.alpha));
}

jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
if (m_api->synchronize_tattoos(actor, false)) {
    return std::unexpected(core::ServiceError{
        core::ServiceErrorCode::synchronizeFailed,
        request.mode == core::UpdateTattooAppearanceMode::updateAndSynchronize
            ? "Tattoo appearance changed but synchronization failed"
            : "Tattoo synchronization failed",
    });
}
```

Use a dedicated JContainer pool name and the existing pool guard so `query_applied_tattoos` storage is released on every return path. For `synchronizeOnly`, skip query and all `JMap` writes.

- [ ] **Step 5: Route the Bridge handler through the service**

Replace direct actor lookup, `JMap` writes, updated flag, and synchronization in `Bridge::handleUpdateTattoo` with:

```cpp
const auto result = m_service.updateAppearance(core::UpdateTattooAppearanceRequest{
    .actorFormId = actorId,
    .runtimeHandle = tattooHandle,
    .color = color,
    .alpha = alpha,
});

if (!result) {
    sendToUI(std::format(
        R"({{"type":"error","message":"{}"}})",
        escapeJSON(result.error().message)));
    return;
}

sendToUI(adapters::toPrismaUpdateTattooSuccessJSON());
```

Do not change `onJSCommand` parsing, task dispatch, request defaults, or error envelope.

- [ ] **Step 6: Build the plugin and run focused tests**

Run:

```powershell
./build.ps1 -Config debug
ctest --test-dir build/debug -R '^(SlaveTatsUICoreTests|PrismaTattooSerializerTests|SlaveTatsAlphaTests)$' --output-on-failure
```

Expected: plugin builds and all three tests pass. Treat compiler errors in the SlaveTatsNG/JContainers calls as evidence to match the existing `handleQueryApplied` and pool-guard patterns exactly; do not weaken validation.

- [ ] **Step 7: Prepare the Runtime and Prisma commit gate**

Run `git diff --check`, stage only Task 2 files, review the staged diff and secret scan, and propose:

```text
feat: share safe tattoo appearance updates
```

Wait for explicit approval before committing.

### Task 3: Add Edit-Session State to the Native Workflow Model

**Files:**
- Modify: `src/native/NativeSlotWorkflowModel.h`
- Modify: `src/native/NativeSlotWorkflowModel.cpp`
- Test: `tests/native/NativeSlotWorkflowModelTests.cpp`

**Interfaces:**
- Consumes: the Core appearance contract from Task 1.
- Produces: `SlotWorkflowScreen::editAppearance` and `SlotWorkflowScreen::savingAppearance`.
- Produces: `AppearanceEditSession`, `SlotAppearanceTicket`, `beginEditAppearance()`, `setEditedAppearance()`, `cancelEditAppearance()`, `confirmAppearanceUpdate()`, `takeAppearanceRequest()`, and `completeAppearanceUpdate()`.

- [ ] **Step 1: Write failing transition and state tests**

Create a fixture slot with all required copied values:

```cpp
TattooEntry{
    .runtimeHandle = 73,
    .section = "Marks",
    .name = "Existing",
    .texturePath = "marks/existing.dds",
    .area = "BODY",
    .slot = 1,
    .color = 0x2468AC,
    .alpha = 0.42F,
}
```

Cover: owned-slot entry, rejection for empty/external/zero-handle slots, initialization, no ticket from local edits, dirty/clean Save state, normalization, Cancel, duplicate Save prevention, success refresh of Body only, write-error retry, sync-error switch to `synchronizeOnly`, retry without a full update, and stale generation rejection.

- [ ] **Step 2: Run the model test and verify Red**

Run:

```powershell
cmake --build build/debug --target NativeSlotWorkflowModelTests
ctest --test-dir build/debug -R '^NativeSlotWorkflowModelTests$' --output-on-failure
```

Expected: compilation fails because edit-session APIs do not exist.

- [ ] **Step 3: Add the edit-session values and public queries**

Add:

```cpp
struct AppearanceEditSession {
    std::uint32_t actorFormId{};
    core::TattooArea area{core::TattooArea::body};
    std::int32_t slot{-1};
    std::int32_t runtimeHandle{};
    std::string texturePath;
    PreviewTattooAppearance original;
    PreviewTattooAppearance edited;
    core::UpdateTattooAppearanceMode mode{
        core::UpdateTattooAppearanceMode::updateAndSynchronize};
};

struct SlotAppearanceTicket {
    std::uint64_t generation{};
    core::UpdateTattooAppearanceRequest request;
};
```

Expose immutable access to the edit session and a `canSaveAppearance()` query. Reuse `PreviewTattooAppearance` rather than creating another color/alpha pair.

- [ ] **Step 4: Implement deterministic transitions and ticket completion**

Construct the session only from the currently selected owned slot. Normalize color and alpha in `setEditedAppearance`. Compare `edited` with `original` for dirty state. On `synchronizeFailed`, keep the session and set its mode to `synchronizeOnly`; on other errors, preserve `updateAndSynchronize`. On success, clear the session, enter Current Slots, and call the existing Selected Area query scheduling path.

- [ ] **Step 5: Run model tests and verify Green**

Run the commands from Step 2.

Expected: all existing and new workflow model cases pass.

- [ ] **Step 6: Prepare the workflow-model commit gate**

Run `git diff --check`, stage only the three Task 3 files, review and secret-scan the staged diff, and propose:

```text
feat: model native appearance editing
```

Wait for explicit approval before committing.

### Task 4: Schedule Appearance Updates on the Game Thread

**Files:**
- Modify: `src/native/NativeSlotWorkflowRuntime.h`
- Modify: `src/native/NativeSlotWorkflowRuntime.cpp`
- Test: `tests/native/NativeSlotWorkflowRuntimeTests.cpp`
- Modify: the native composition root that constructs `NativeSlotWorkflowRuntime` (locate the existing constructor call with `rg -n "NativeSlotWorkflowRuntime" src` and change that call only)

**Interfaces:**
- Consumes: `SlotAppearanceTicket` and model completion API from Task 3.
- Produces: `SlotAppearanceOperation = std::function<core::UpdateTattooAppearanceResult(const core::UpdateTattooAppearanceRequest&)>`.
- Produces: a constructor parameter wired to `SlaveTatsService::updateAppearance`.

- [ ] **Step 1: Extend the fixture and write failing scheduling tests**

Add an appearance lambda between the existing remove operation and scheduler:

```cpp
[this](const UpdateTattooAppearanceRequest& request) {
    ++appearanceCount;
    appearanceRequest = request;
    if (appearanceThrows) {
        throw std::runtime_error("appearance update failed");
    }
    return UpdateTattooAppearanceResult(UpdateTattooAppearanceSuccess{
        .actorFormId = request.actorFormId,
        .runtimeHandle = request.runtimeHandle,
    });
}
```

Test full-update scheduling, synchronization-only scheduling, duplicate pump suppression, operation exception, scheduler rejection, and release of the in-flight guard after every completion path.

- [ ] **Step 2: Run the runtime-coordinator test and verify Red**

Run:

```powershell
cmake --build build/debug --target NativeSlotWorkflowRuntimeTests
ctest --test-dir build/debug -R '^NativeSlotWorkflowRuntimeTests$' --output-on-failure
```

Expected: compilation fails because the coordinator has no appearance operation.

- [ ] **Step 3: Implement appearance scheduling**

Add `m_updateAppearance`, constructor injection, `scheduleAppearance`, and this `pump()` branch after existing mutations and before clearing `m_inFlight`:

```cpp
if (auto appearance = m_model.takeAppearanceRequest()) {
    scheduleAppearance(std::move(*appearance));
    return;
}
```

Map caught exceptions and scheduler rejection to `updateFailed` for full update and `synchronizeFailed` for synchronization-only mode. Always call `completeAppearanceUpdate` with the ticket generation and release the guard.

- [ ] **Step 4: Wire the composition root to the shared service**

Pass a lambda with the exact contract:

```cpp
[&service](const core::UpdateTattooAppearanceRequest& request) {
    return service.updateAppearance(request);
}
```

Do not introduce actor, JContainers, or synchronization logic at this layer.

- [ ] **Step 5: Run focused tests and a Debug plugin build**

Run:

```powershell
cmake --build build/debug --target NativeSlotWorkflowRuntimeTests
ctest --test-dir build/debug -R '^(NativeSlotWorkflowModelTests|NativeSlotWorkflowRuntimeTests)$' --output-on-failure
./build.ps1 -Config debug
```

Expected: focused tests and plugin build pass.

- [ ] **Step 6: Prepare the coordinator commit gate**

Run `git diff --check`, stage only Task 4 files, review and secret-scan the staged diff, and propose:

```text
feat: schedule native appearance updates
```

Wait for explicit approval before committing.

### Task 5: Render Edit Appearance and Reuse Presentation Helpers

**Files:**
- Modify: `src/native/OfficialMenuFrameworkAdapter.h`
- Modify: `src/native/OfficialMenuFrameworkAdapter.cpp`
- Test: `tests/native/OfficialMenuFrameworkAdapterTests.cpp`

**Interfaces:**
- Consumes: model edit-session commands and queries from Task 3.
- Produces: `isAppearanceSaveEnabled(SlotWorkflowScreen, const AppearanceEditSession*)`.
- Produces: an Edit Appearance renderer using the existing RGB conversion and thumbnail presentation.

- [ ] **Step 1: Write failing adapter-helper tests**

Add cases proving:

```cpp
expect(!isAppearanceSaveEnabled(
    SlotWorkflowScreen::editAppearance,
    &unchangedSession), "unchanged appearance must disable Save");
expect(isAppearanceSaveEnabled(
    SlotWorkflowScreen::editAppearance,
    &changedSession), "changed appearance must enable Save");
expect(!isAppearanceSaveEnabled(
    SlotWorkflowScreen::savingAppearance,
    &changedSession), "saving appearance must disable duplicate Save");
```

Also retain existing RGB channel-order tests and add assertions that the edit thumbnail inputs use the session's edited color, alpha, and texture path.

- [ ] **Step 2: Run the adapter test and verify Red**

Run:

```powershell
cmake --build build/debug --target OfficialMenuFrameworkAdapterTests
ctest --test-dir build/debug -R '^OfficialMenuFrameworkAdapterTests$' --output-on-failure
```

Expected: compilation fails because the helper is absent.

- [ ] **Step 3: Extract the smallest reusable appearance presentation helpers**

Keep `tattooColorComponents` and `tattooColorValue` as the single RGB conversion path. Add only the helper needed to map model state to Save enablement; do not create a second editor model inside the adapter.

- [ ] **Step 4: Add Edit Appearance to Slot Actions and render the screen**

Add the action only for the already validated SlaveTats-managed target. Render:

```cpp
ImGuiMCP::ColorEdit3("Color", colorValues, ImGuiColorEditFlags_NoInputs);
ImGuiMCP::SliderFloat("Alpha", &alpha, 0.0F, 1.0F, "%.2f");
```

On changes, call `workflow.setEditedAppearance(...)`. Tint the existing Current Slot thumbnail using edited color and alpha. Render `Cancel` before any Save, `Save` for full-update mode, and `Retry Sync` after `synchronizeFailed`. While `savingAppearance`, disable controls and submission buttons. Route the new screens from the existing workflow-screen switch.

- [ ] **Step 5: Run focused Native tests and Debug build**

Run:

```powershell
cmake --build build/debug --target NativeSlotWorkflowModelTests NativeSlotWorkflowRuntimeTests OfficialMenuFrameworkAdapterTests
ctest --test-dir build/debug -R '^(NativeSlotWorkflowModelTests|NativeSlotWorkflowRuntimeTests|OfficialMenuFrameworkAdapterTests)$' --output-on-failure
./build.ps1 -Config debug
```

Expected: all focused tests and the plugin build pass.

- [ ] **Step 6: Prepare the Native UI commit gate**

Run `git diff --check`, stage only the three Task 5 files, review and secret-scan the staged diff, and propose:

```text
feat: add native edit appearance screen
```

Wait for explicit approval before committing.

### Task 6: Complete Cross-Configuration Verification

**Files:**
- Verify only; modify scoped source/tests only if a real failure requires a fix.
- Reference: `docs/superpowers/specs/2026-09-11-native-edit-appearance-design.md`

**Interfaces:**
- Consumes: all prior tasks.
- Produces: fresh build, test, hygiene, and review evidence for the completed feature.

- [ ] **Step 1: Build and test Debug from the project script**

Run:

```powershell
./build.ps1 -Config debug
ctest --test-dir build/debug --output-on-failure
```

Expected: Debug plugin builds and the complete Debug CTest suite passes.

- [ ] **Step 2: Build and test Release from the project script**

Run:

```powershell
./build.ps1 -Config release
ctest --test-dir build/release --output-on-failure
```

Expected: Release plugin builds and the complete Release CTest suite passes.

- [ ] **Step 3: Run repository hygiene checks**

Run:

```powershell
git diff --check
git status --short
git diff --stat
rg -n -i "api[_-]?key|secret|token|password|credential|BEGIN (RSA|OPENSSH|PRIVATE)" src tests docs/superpowers
```

Expected: no whitespace errors, no generated build files in the diff, and no introduced secret material. Review every search match as code/document vocabulary rather than assuming it is safe.

- [ ] **Step 4: Review the complete change against the fixed point**

Use `code-review` with fixed point `d6cabb992b2c51a25962c20f84b75a70c60f7628`. Confirm all spec requirements, error paths, lifetime assumptions, and Prisma compatibility. Fix genuine findings with a focused failing test first, then rerun Steps 1 through 3.

- [ ] **Step 5: Prepare the final commit gate if verification fixes exist**

If Step 4 required changes, stage only those scoped fixes, show the complete staged diff and propose an exact Conventional Commit message matching the fix. Wait for explicit approval before committing. If no fixes exist, do not create an empty verification commit.

- [ ] **Step 6: Stop at the deployment gate**

Report the Debug and Release test totals, resulting Release DLL path and SHA-256, working-tree status, and commits created with approval. Do not copy the DLL to the MO2 mod directory until the user gives a fresh explicit Deploy approval.
