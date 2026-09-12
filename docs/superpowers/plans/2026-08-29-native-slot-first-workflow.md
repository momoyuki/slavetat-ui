# Native Slot-First Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Open Native SlaveTatsUI on the Player's current slots, let the user choose a mutable slot, pick a catalog tattoo, preview the target, and explicitly apply it through a service shared with PrismaUI.

**Architecture:** Add transport-independent slot/query/apply values to core, move Skyrim-specific slot operations behind `ITattooRuntime`, and keep Prisma JSON serialization in adapters. A native workflow model owns `CurrentSlots -> Picker -> Preview -> Applying` state, while injected game-thread operations and the existing bounded thumbnail runtime perform external work.

**Tech Stack:** C++23, CommonLibSSE-NG, SlaveTatsNG API, JContainers, SKSE task interface, SKSE Menu Framework ImGui API, Direct3D 11, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-08-29-native-slot-first-workflow-design.md`

## Global Constraints

- Keep F1 SKSE Menu Framework and F8 PrismaUI behavior operational.
- Target the Player only in this plan; use form ID `0x14` at the composition boundary.
- Support exactly BODY, FACE, HANDS, and FEET as typed areas.
- Render six slot or catalog cards per page in a two-column by three-row grid without vertical scrolling.
- Never query SlaveTatsNG/JContainers, read `skee64.ini`, or mutate an actor from the ImGui render callback.
- Never select or mutate an external overlay slot.
- Require an explicit Apply confirmation; selecting a catalog thumbnail must not mutate the actor.
- Use fixed apply defaults: domain `default`, white `0xFFFFFF`, and alpha `1.0F`; `TattooDefinition` has no domain, color, or alpha fields.
- Preserve picker search, filters, and page across Back, Cancel, and Apply.
- Reuse the native capacity-12, two-minute idle D3D11 cache; do not change Prisma thumbnail transport.
- Use `apply_patch` for edits and `build.ps1` for compiler-environment setup.
- Before every commit, show the staged diff and proposed Conventional Commit message and obtain explicit approval.

## File Structure

- `src/core/TattooModels.h`: shared slot, apply, success, and error value types.
- `src/core/ITattooRuntime.h`: typed runtime operations for querying and applying slots.
- `src/core/SlaveTatsService.h/.cpp`: availability and request validation shared by Native and Prisma.
- `src/runtime/OverlaySlotConfiguration.h/.cpp`: area-to-INI mapping and cached configured slot counts.
- `src/runtime/SlaveTatsRuntime.h/.cpp`: RE, SlaveTatsNG, JContainers, external-slot, and synchronization adapter.
- `src/adapters/PrismaSlotSerializer.h/.cpp`: existing Prisma slot/success JSON shapes.
- `src/native/NativeSlotWorkflowModel.h/.cpp`: deterministic slot/picker/preview/apply state machine.
- `src/native/NativeSlotWorkflowRuntime.h/.cpp`: injected scheduling and service completion coordination.
- `src/native/NativeThumbnailController.h/.cpp`: presentation-neutral visible-path synchronization.
- `src/native/NativeThumbnailRuntime.h/.cpp`: catalog and slot visible-path overloads sharing one source/cache.
- `src/native/OfficialMenuFrameworkAdapter.h/.cpp`: Current Slots, Picker, and Preview rendering only.
- `src/Bridge.h/.cpp`: transitional shared-service accessor and Prisma delegation.
- `src/main.cpp`: process-lifetime native workflow composition.
- Corresponding tests under `tests/core`, `tests/runtime`, `tests/adapters`, and `tests/native`.

---

### Task 1: Add Shared Slot Query and Apply Contracts

**Files:**
- Modify: `src/core/TattooModels.h`
- Modify: `src/core/ITattooRuntime.h`
- Modify: `src/core/SlaveTatsService.h`
- Modify: `src/core/SlaveTatsService.cpp`
- Modify: `tests/core/SlaveTatsServiceTests.cpp`

**Interfaces:**
- Consumes: existing `TattooEntry`, `ServiceError`, `ITattooRuntime::apiAvailable()`, and `jContainersReady()`.
- Produces: `TattooArea`, `SlotOccupancy`, `TattooSlot`, `TattooSlots`, `ApplyTattooRequest`, `ApplyTattooSuccess`, `TattooSlotsResult`, `ApplyTattooResult`, `ITattooRuntime::querySlots`, `ITattooRuntime::applyToSlot`, and matching service methods.

- [ ] **Step 1: Write failing core service tests**

Extend `FakeTattooRuntime` with captured slot/apply requests and add literal tests for availability short-circuiting, request forwarding, invalid actor ID, invalid negative slot, and runtime error propagation:

```cpp
TattooSlotsResult querySlots(std::uint32_t actorFormId, TattooArea area) override {
    queriedActor = actorFormId;
    queriedArea = area;
    ++slotQueryCount;
    return slotQueryResult;
}

ApplyTattooResult applyToSlot(const ApplyTattooRequest& request) override {
    appliedRequest = request;
    ++applyCount;
    return applyResult;
}

void validSlotQueryIsForwardedExactlyOnce() {
    FakeTattooRuntime runtime;
    runtime.slotQueryResult = TattooSlots{
        .actorFormId = 0x14,
        .area = TattooArea::body,
        .configuredCount = 12,
        .slots = {{.index = 0, .occupancy = SlotOccupancy::empty}},
    };
    SlaveTatsService service(runtime);

    const auto result = service.querySlots(0x14, TattooArea::body);

    expect(result.has_value(), "expected slot query success");
    expect(runtime.slotQueryCount == 1 && runtime.queriedActor == 0x14,
        "expected exactly one Player slot query");
}
```

- [ ] **Step 2: Run the core target and verify RED**

Run:

```powershell
.\build.ps1 -Config debug
ctest --test-dir build\debug -R SlaveTatsUICoreTests --output-on-failure
```

Expected: compilation fails because slot types and virtual methods do not exist.

- [ ] **Step 3: Add minimal typed contracts**

Add these contracts, using `std::optional<TattooEntry>` only for SlaveTats-owned slots:

```cpp
enum class TattooArea { body, face, hands, feet };
enum class SlotOccupancy { empty, slaveTats, external };

struct TattooSlot {
    std::int32_t index{-1};
    SlotOccupancy occupancy{SlotOccupancy::empty};
    std::optional<TattooEntry> tattoo;
};

struct TattooSlots {
    std::uint32_t actorFormId{};
    TattooArea area{TattooArea::body};
    std::int32_t configuredCount{};
    std::vector<TattooSlot> slots;
};

struct ApplyTattooRequest {
    std::uint32_t actorFormId{};
    TattooArea area{TattooArea::body};
    std::int32_t slot{-1};
    std::string domain{"default"};
    std::string section;
    std::string name;
    std::int32_t color{0xFFFFFF};
    float alpha{1.0F};
};

struct ApplyTattooSuccess {
    std::uint32_t actorFormId{};
    TattooArea area{TattooArea::body};
    std::int32_t slot{-1};
    std::string section;
    std::string name;
};
```

Add service errors `actorNotFound`, `invalidArea`, `invalidSlot`, `externalSlot`, `slotQueryFailed`, `tattooNotFound`, `applyFailed`, and `synchronizeFailed`. Validate API/JContainers availability, nonzero actor ID, nonnegative slot, nonempty section/name, and alpha in `[0.0F, 1.0F]` before forwarding.

- [ ] **Step 4: Run focused GREEN verification**

Run the core target directly and through CTest. Expected: all existing and new core cases pass with no warnings.

- [ ] **Step 5: Present commit gate**

Stage only the five Task 1 files, show `git diff --cached`, and propose:

```text
feat: add shared tattoo slot contracts
```

Do not commit until explicit approval.

---

### Task 2: Implement Configured Slot Counts and Runtime Slot Operations

**Files:**
- Create: `src/runtime/OverlaySlotConfiguration.h`
- Create: `src/runtime/OverlaySlotConfiguration.cpp`
- Modify: `src/runtime/SlaveTatsRuntime.h`
- Modify: `src/runtime/SlaveTatsRuntime.cpp`
- Create: `tests/runtime/OverlaySlotConfigurationTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 core contracts and existing bound SlaveTats/JContainers interfaces.
- Produces: `OverlaySlotConfiguration::count(TattooArea)`, `SlaveTatsRuntime::querySlots`, and `SlaveTatsRuntime::applyToSlot`.

- [ ] **Step 1: Write failing configuration tests**

Use an injected reader so tests never read the developer's INI:

```cpp
using IniIntegerReader = std::function<int(
    std::wstring_view section,
    std::wstring_view key,
    int fallback)>;

OverlaySlotConfiguration configuration(
    [](std::wstring_view section, std::wstring_view, int fallback) {
        if (section == L"Overlays/Body") return 18;
        return fallback;
    });

expect(configuration.count(TattooArea::body) == 18,
    "expected configured BODY overlay count");
expect(configuration.count(TattooArea::face) == 3,
    "expected FACE fallback count");
```

- [ ] **Step 2: Register the test and verify RED**

Add `OverlaySlotConfigurationTests` to CMake, build Debug, and run only that test. Expected: missing header/type failure.

- [ ] **Step 3: Implement the configuration deep module**

Map BODY to `Overlays/Body` with fallback 12 and FACE/HANDS/FEET to their matching sections with fallback 3. Cache one count per area after the first read. The production constructor resolves `<Skyrim>\Data\SKSE\Plugins\skee64.ini` and wraps `GetPrivateProfileIntW`; tests use the injected constructor.

- [ ] **Step 4: Implement `SlaveTatsRuntime::querySlots`**

Port the current `Bridge::handleQuerySlots` behavior without JSON:

```cpp
core::TattooSlotsResult SlaveTatsRuntime::querySlots(
    std::uint32_t actorFormId,
    core::TattooArea area);
```

Resolve the actor, query external slots into a scoped pool, iterate exactly `configuredCount` slots, copy applied tattoo fields into `TattooEntry`, and return entries ordered by slot index. Normalize legacy color `0` to `0xFFFFFF` and always clean the external-slot pool through an RAII guard.

- [ ] **Step 5: Implement `SlaveTatsRuntime::applyToSlot`**

Port the existing domain re-query and template color/alpha restore. Before mutation, re-query external slots for the target area and return `externalSlot` if the target was claimed after the UI snapshot. After `add_and_get_tattoo`, mark `.SlaveTats.updated`, call `synchronize_tattoos` once, and distinguish apply failure from synchronize failure.

- [ ] **Step 6: Run Task 2 and complete Debug verification**

Run `OverlaySlotConfigurationTests`, `SlaveTatsUICoreTests`, then all Debug CTest tests. Expected: all pass.

- [ ] **Step 7: Present commit gate**

Show the staged diff and propose:

```text
feat: implement SlaveTats slot runtime
```

Do not commit until explicit approval.

---

### Task 3: Route Prisma Slot Queries and Apply Through the Shared Service

**Files:**
- Create: `src/adapters/PrismaSlotSerializer.h`
- Create: `src/adapters/PrismaSlotSerializer.cpp`
- Create: `tests/adapters/PrismaSlotSerializerTests.cpp`
- Modify: `src/Bridge.h`
- Modify: `src/Bridge.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TattooSlots`, `ApplyTattooSuccess`, and Task 1 service methods.
- Produces: `toPrismaSlotsJSON(const TattooSlots&)`, `toPrismaApplySuccessJSON(const ApplyTattooSuccess&)`, and a transitional `Bridge::tattooService()` accessor for native composition.

- [ ] **Step 1: Write failing serializer regression tests**

Assert complete literal payloads for empty, external, and SlaveTats slots, including existing field names and numeric formats:

```cpp
expect(toPrismaSlotsJSON(slots) ==
    R"({"type":"slots","area":"BODY","maxSlots":3,"slots":[{"slot":0,"occupied":false},{"slot":1,"occupied":true,"external":true,"name":"[External]"},{"slot":2,"occupied":true,"name":"Rose","section":"Pack","texture":"rose.dds","color":16777215,"alpha":1.00,"handle":42}]})",
    "expected unchanged Prisma slots payload");
```

- [ ] **Step 2: Register the serializer target and verify RED**

Add a dedicated `PrismaSlotSerializerTests` target. Expected: missing serializer failure.

- [ ] **Step 3: Implement serialization and Bridge delegation**

Keep escaping private to the adapter. Replace the bodies of `handleQuerySlots`, `handleQueryAllSlots`, and `handleApplyToSlot` with calls to `m_service`. Convert errors to the existing `{type:"error",message:"..."}` envelope and preserve success payloads.

Expose only:

```cpp
[[nodiscard]] core::SlaveTatsService& tattooService() noexcept;
```

This accessor is transitional composition glue; native modules must depend on callables or core interfaces, never on `Bridge`.

- [ ] **Step 4: Verify focused tests and Prisma compatibility**

Run `PrismaSlotSerializerTests`, `PrismaTattooSerializerTests`, and `SlaveTatsUICoreTests`. Build the plugin target to prove Bridge/runtime linkage.

- [ ] **Step 5: Present commit gate**

Show the staged diff and propose:

```text
refactor: route Prisma slot operations through service
```

Do not commit until explicit approval.

---

### Task 4: Add the Native Slot Workflow State Machine

**Files:**
- Create: `src/native/NativeSlotWorkflowModel.h`
- Create: `src/native/NativeSlotWorkflowModel.cpp`
- Create: `tests/native/NativeSlotWorkflowModelTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: core slot/apply contracts and an existing `NativeCatalogBrowserModel&`.
- Produces: deterministic navigation, paging, request tickets, completion methods, and immutable presentation accessors.

- [ ] **Step 1: Write failing state-machine tests**

Cover initial Player/BODY query, repeated-read idempotence, area caching, per-area pages, external-slot rejection, empty/owned target selection, catalog state retention, preview without mutation, duplicate Apply prevention, success refresh, failure retry, and stale generation rejection.

Use literal transitions such as:

```cpp
NativeSlotWorkflowModel model(catalog);
const auto initial = model.takeSlotQuery();
expect(initial && initial->actorFormId == 0x14 && initial->area == TattooArea::body,
    "expected one initial Player BODY query");
expect(!model.takeSlotQuery(), "expected repeated reads not to query again");

model.completeSlotQuery(initial->generation, bodySlots);
expect(model.selectSlot(2), "expected empty slot to become a target");
expect(model.screen() == SlotWorkflowScreen::picker,
    "expected target selection to open Picker");

model.selectTattoo(definition);
expect(model.screen() == SlotWorkflowScreen::preview,
    "expected thumbnail selection to preview without apply");
expect(model.takeApplyRequest().has_value() == false,
    "expected no mutation before explicit confirmation");
```

- [ ] **Step 2: Register the target and verify RED**

Add `NativeSlotWorkflowModelTests` with `NativeSlotWorkflowModel.cpp`, `NativeCatalogBrowserModel.cpp`, and `TattooRepository.cpp`. Build Debug and run the target. Expected: missing model failure.

- [ ] **Step 3: Implement the minimal state contract**

Expose this test-facing API:

```cpp
enum class SlotWorkflowScreen { currentSlots, picker, preview, applying };

struct SlotQueryTicket {
    std::uint64_t generation{};
    std::uint32_t actorFormId{};
    core::TattooArea area{core::TattooArea::body};
};

struct SlotApplyTicket {
    std::uint64_t generation{};
    core::ApplyTattooRequest request;
};

class NativeSlotWorkflowModel {
public:
    static constexpr std::size_t kPageSize = 6;
    explicit NativeSlotWorkflowModel(NativeCatalogBrowserModel& catalog);
    void start();
    void selectArea(core::TattooArea area);
    void refreshSelectedArea();
    void previousSlotPage();
    void nextSlotPage();
    void setSlotPageNumber(std::size_t oneBasedPage);
    [[nodiscard]] bool selectSlot(std::int32_t slot);
    void backToSlots();
    void selectTattoo(const repository::TattooDefinition& tattoo);
    void cancelPreview();
    [[nodiscard]] bool confirmApply();
    [[nodiscard]] std::optional<SlotQueryTicket> takeSlotQuery();
    [[nodiscard]] std::optional<SlotApplyTicket> takeApplyRequest();
    void completeSlotQuery(std::uint64_t generation, core::TattooSlotsResult result);
    void completeApply(std::uint64_t generation, core::ApplyTattooResult result);
};
```

Store complete results per area and derive six-entry pages in the model. Store only copied `TattooDefinition` preview data. Increment the generation whenever a request becomes obsolete.

- [ ] **Step 4: Run GREEN and mutation checks**

Run the focused target. Temporarily verify that removing generation checks, external rejection, or applying-state suppression makes a named test fail; restore each mutation and rerun GREEN.

- [ ] **Step 5: Present commit gate**

Show the staged diff and propose:

```text
feat: add native slot workflow model
```

Do not commit until explicit approval.

---

### Task 5: Add Async Slot Coordination and Presentation-Neutral Thumbnails

**Files:**
- Create: `src/native/NativeSlotWorkflowRuntime.h`
- Create: `src/native/NativeSlotWorkflowRuntime.cpp`
- Create: `tests/native/NativeSlotWorkflowRuntimeTests.cpp`
- Modify: `src/native/NativeThumbnailController.h`
- Modify: `src/native/NativeThumbnailController.cpp`
- Modify: `src/native/NativeThumbnailRuntime.h`
- Modify: `src/native/NativeThumbnailRuntime.cpp`
- Modify: `tests/native/NativeThumbnailControllerTests.cpp`
- Modify: `tests/native/NativeThumbnailRuntimeTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 4 tickets, injected query/apply operations, scheduler, and existing thumbnail source/cache.
- Produces: one-operation-at-a-time game-thread pumping and generic visible texture synchronization.

- [ ] **Step 1: Write failing coordinator tests**

Define injectable operations and prove that pumping schedules one query, repeated pumps do not duplicate it, completion updates the model, Apply schedules only after confirmation, and stale completions are ignored:

```cpp
using SlotQueryOperation = std::function<core::TattooSlotsResult(
    std::uint32_t, core::TattooArea)>;
using SlotApplyOperation = std::function<core::ApplyTattooResult(
    const core::ApplyTattooRequest&)>;

NativeSlotWorkflowRuntime runtime(
    model,
    [&](std::uint32_t actor, TattooArea area) { return query(actor, area); },
    [&](const ApplyTattooRequest& request) { return apply(request); },
    [&](NativeSlotTask task) { scheduled.push_back(std::move(task)); });

runtime.pump();
runtime.pump();
expect(scheduled.size() == 1, "expected one in-flight slot operation");
```

- [ ] **Step 2: Write failing generic-thumbnail tests**

Add a visible-path synchronization test independent of `TattooPage`:

```cpp
const auto epoch = std::make_shared<int>(1);
const std::vector<std::string> paths{"slot/a.dds", "slot/b.dds"};
controller.synchronize(epoch, paths, lookup);
expect(controller.views().size() == 2,
    "expected slot texture paths accepted without repository entries");
```

Retain a catalog wrapper test to ensure existing picker behavior is unchanged.

- [ ] **Step 3: Verify RED for both targets**

Build Debug and run `NativeSlotWorkflowRuntimeTests`, `NativeThumbnailControllerTests`, and `NativeThumbnailRuntimeTests`. Expected: missing coordinator and generic overload failures.

- [ ] **Step 4: Implement the coordinator**

Use `NativeSlotTask = std::function<void()>` and `NativeSlotScheduler`. `pump()` takes at most one pending ticket and schedules one closure. The closure runs the injected operation, completes the model with the captured generation, and clears the in-flight flag through an exception-safe guard. Convert exceptions to `ServiceErrorCode::slotQueryFailed` or `applyFailed`.

- [ ] **Step 5: Generalize thumbnail synchronization minimally**

Replace repository snapshot ownership inside the controller with an opaque shared epoch and visible paths:

```cpp
using NativeThumbnailEpoch = std::shared_ptr<const void>;

void synchronize(
    NativeThumbnailEpoch epoch,
    std::span<const std::string> texturePaths,
    const NativeThumbnailCacheLookup& lookup);
```

Keep `NativeThumbnailRuntime::synchronize(snapshot, page)` as a compatibility wrapper that builds current catalog paths. Add a second overload accepting `NativeThumbnailEpoch` and `std::span<const std::string>`. One source, manager, negative cache, capacity, and TTL remain shared.

- [ ] **Step 6: Run focused and full Debug verification**

Run the three focused targets, then the complete Debug CTest suite. Expected: catalog thumbnails remain green and slot requests never exceed six visible paths.

- [ ] **Step 7: Present commit gate**

Show the staged diff and propose:

```text
feat: add native slot workflow runtime
```

Do not commit until explicit approval.

---

### Task 6: Render Current Player Slots with Pagination and Thumbnails

**Files:**
- Modify: `src/native/OfficialMenuFrameworkAdapter.h`
- Modify: `src/native/OfficialMenuFrameworkAdapter.cpp`
- Modify: `tests/native/OfficialMenuFrameworkAdapterTests.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 4-5 model/runtime and generic thumbnail synchronization.
- Produces: Player/BODY default entry, area tabs, six-card slot pages, Refresh, disabled external cards, and target navigation into Picker.

- [ ] **Step 1: Write failing adapter layout tests**

Extract pure presentation helpers and test literal behavior:

```cpp
expect(slotCardTreatment(SlotOccupancy::empty) == SlotCardTreatment::add,
    "expected empty slot Add treatment");
expect(slotCardTreatment(SlotOccupancy::slaveTats) == SlotCardTreatment::replace,
    "expected SlaveTats slot Replace treatment");
expect(slotCardTreatment(SlotOccupancy::external) == SlotCardTreatment::disabled,
    "expected external slot disabled treatment");

const auto page = calculateSlotPage(12, 0, 6);
expect(page.pageCount == 2 && page.begin == 0 && page.end == 6,
    "expected first BODY page to contain six slots");
```

Also test area labels, clamped pagination, and visible texture-path collection excluding empty/external slots.

- [ ] **Step 2: Verify RED**

Build Debug and run `OfficialMenuFrameworkAdapterTests`. Expected: missing slot presentation helpers.

- [ ] **Step 3: Split rendering by workflow screen**

Change the adapter entry point to:

```cpp
static void renderFoundation(
    NativeSlotWorkflowModel& workflow,
    NativeSlotWorkflowRuntime& slotRuntime,
    NativeCatalogBrowserModel& catalog,
    NativeThumbnailRuntime& thumbnails,
    const std::function<void()>& close);
```

Extract private/free rendering functions for Current Slots, Picker, and Preview so later UX changes do not alter service/runtime code. This task implements Current Slots and a temporary Picker heading/back control; Task 7 completes Picker/Preview.

- [ ] **Step 4: Render Current Slots**

On each frame, call `slotRuntime.pump()`, read model state, synchronize thumbnails with only the visible owned-slot paths, then render:

- `Current Tattoos — Player` and Refresh;
- BODY/FACE/HANDS/FEET tabs;
- two-by-three cards using the existing no-scroll grid calculation;
- current image and hover name for owned slots;
- Add affordance for empty slots;
- locked disabled treatment for external slots;
- Prev/Page/Next and Close footer.

Clicks call model intent methods only. The render function must not invoke service operations.

- [ ] **Step 5: Compose process-lifetime workflow objects**

In `main.cpp`, create the model and runtime after existing catalog/thumbnail globals. Inject operations as lambdas calling `Bridge::get()->tattooService()` and schedule through `SKSE::GetTaskInterface()->AddTask`. Call `workflow.start()` after SlaveTatsNG/JContainers readiness is available; unavailable results remain retryable through Refresh.

- [ ] **Step 6: Verify Task 6**

Run adapter, workflow, thumbnail, core, and Prisma serializer targets. Build Debug and Release and run both full CTest suites.

- [ ] **Step 7: Present commit and Deploy gates**

Propose:

```text
feat: render native Player tattoo slots
```

After commit approval, request separate Deploy approval. In game, verify area counts, two BODY pages, three-slot areas, current thumbnails, disabled external slots, Refresh, no repeated queries, and unchanged F8 PrismaUI.

---

### Task 7: Complete Target-Aware Picker, Preview, and Confirmed Apply

**Files:**
- Modify: `src/native/OfficialMenuFrameworkAdapter.h`
- Modify: `src/native/OfficialMenuFrameworkAdapter.cpp`
- Modify: `tests/native/OfficialMenuFrameworkAdapterTests.cpp`
- Modify: `tests/native/NativeSlotWorkflowModelTests.cpp`

**Interfaces:**
- Consumes: target/picker/preview/apply state from Task 4 and existing catalog browser controls.
- Produces: complete `CurrentSlots -> Picker -> Preview -> Applying -> CurrentSlots` user workflow.

- [ ] **Step 1: Extend failing workflow tests**

Add end-to-end model cases proving:

```cpp
model.selectSlot(2);
const auto filterBefore = catalog.filter();
const auto pageBefore = catalog.page().pageIndex;
model.selectTattoo(tattoo);
expect(model.screen() == SlotWorkflowScreen::preview,
    "expected explicit preview before apply");
expect(model.confirmApply(), "expected first confirmation accepted");
expect(!model.confirmApply(), "expected duplicate confirmation rejected");

model.completeApply(ticket.generation, ApplyTattooSuccess{
    .actorFormId = 0x14,
    .area = TattooArea::body,
    .slot = 2,
    .section = "LewdMarks",
    .name = "Corruption",
});
expect(model.screen() == SlotWorkflowScreen::currentSlots,
    "expected success to return to current slots");
expect(catalog.filter().search == filterBefore.search &&
           catalog.filter().sourceId == filterBefore.sourceId &&
           catalog.filter().section == filterBefore.section &&
           catalog.filter().area == filterBefore.area &&
           catalog.page().pageIndex == pageBefore,
    "expected picker filter and page state preserved after apply");
```

Add failure/retry, Cancel, Back, and stale completion cases.

- [ ] **Step 2: Add failing adapter behavior tests**

Test pure helpers for target labels (`Player / BODY / Slot 2`), preview button labels, Apply-disabled state, and picker visible paths. Ensure selecting a thumbnail returns presentation intent only and cannot call an operation directly.

- [ ] **Step 3: Verify RED**

Run workflow and adapter tests. Expected: incomplete picker/preview behavior failures.

- [ ] **Step 4: Complete Picker rendering**

Reuse existing Filters, source/section/area selectors, two-by-three thumbnail grid, tooltips, and pagination. Add a Back control and persistent target label. Clicking a card calls `workflow.selectTattoo(tattoo)` and does not apply.

- [ ] **Step 5: Implement Preview and Applying rendering**

Render the selected image, tattoo name, and exact target. Provide Cancel and `Apply to Slot N`. On Apply, call `workflow.confirmApply()`; subsequent frames let `slotRuntime.pump()` schedule the request. Disable Apply while `screen() == applying`. On failure, show the service message and Retry/Cancel while preserving target and selected tattoo.

- [ ] **Step 6: Run complete automated verification**

Run:

```powershell
git diff --check
.\build.ps1 -Config debug
ctest --test-dir build\debug --output-on-failure
.\build.ps1 -Config release
ctest --test-dir build\release --output-on-failure
```

Scan the complete diff for secrets, machine-specific paths, debug markers, and accidental `.superpowers/` files.

- [ ] **Step 7: Perform code review and present commit gate**

Use the `code-review` skill against the Task 7 fixed point. Resolve all correctness findings, rerun focused and full verification, show the final diff, and propose:

```text
feat: add native slot-first tattoo apply flow
```

Do not commit until explicit approval.

- [ ] **Step 8: Request Deploy approval and run in-game acceptance**

Back up the installed DLL, deploy the verified Release DLL, and verify all 13 in-game acceptance steps in the spec. Record log evidence separately from automated test evidence. If the UX is awkward, revise only model transitions or adapter layout; keep shared service/runtime contracts stable.

---

## Final Completion Gate

The feature is complete only when every task commit is approved, Debug and Release suites pass, the deployed DLL hash matches the built Release DLL, and the in-game acceptance checklist passes. PrismaUI retirement remains out of scope.
