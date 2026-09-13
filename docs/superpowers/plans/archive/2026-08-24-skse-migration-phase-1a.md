# SKSE Migration Phase 1A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the available-tattoo query from the Prisma-specific `Bridge` into a typed, UI-independent core service while preserving the existing PrismaUI payload and behavior.

**Architecture:** Introduce a small `ITattooRuntime` seam between UI-independent orchestration and the Skyrim/JContainers implementation. `SlaveTatsRuntime` owns SlaveTatsNG/JContainers binding and copies pooled JContainer records into value-type `TattooEntry` objects; `SlaveTatsService` exposes typed results; `Bridge` remains the Prisma transport adapter and serializes those values back to the existing JSON shape.

**Tech Stack:** C++23, CommonLibSSE-NG, SlaveTatsNG interface v1, JContainers reflection API, nlohmann/json, CMake/CTest, MSVC/Ninja.

**Spec:** `SKSE_MENU_MIGRATION.md`

## Global Constraints

- Keep PrismaUI operational and do not remove it in this phase.
- Do not change SlaveTatsNG behavior or public APIs.
- Do not retain JContainer handles as durable repository identity; copy metadata before cleaning the pool.
- Keep `sourceFile`, `sourceId`, and `packName` distinct from `section`, even when source metadata is not yet available.
- Core service headers must not include `PrismaUI_API.h`.
- Preserve the existing Prisma `available` payload fields and error messages.
- Do not change texture loading, actor discovery, slot semantics, or mutation flows in this phase.
- Do not commit without explicit user approval; commit steps below are proposed checkpoints only.

---

## File Structure

- `src/core/TattooModels.h`: UI-independent tattoo value types and service error/result types.
- `src/core/ITattooRuntime.h`: narrow runtime seam consumed by `SlaveTatsService` and implemented by the Skyrim adapter.
- `src/core/SlaveTatsService.h`: UI-independent query interface used by frontends.
- `src/core/SlaveTatsService.cpp`: availability validation and typed query orchestration.
- `src/runtime/SlaveTatsRuntime.h`: production SlaveTatsNG/JContainers runtime adapter and temporary legacy accessors used by handlers not migrated yet.
- `src/runtime/SlaveTatsRuntime.cpp`: API binding, JContainers initialization, pooled query execution, and value copying.
- `tests/core/SlaveTatsServiceTests.cpp`: executable tests for service behavior using a real fake implementation of the runtime seam.
- `src/Bridge.h`: own the runtime/service and expose a typed Prisma serializer overload.
- `src/Bridge.cpp`: forward lifecycle binding, use the service for `queryAvailable`, and preserve JSON transport.
- `CMakeLists.txt`: compile the new production files and register the core test executable with CTest.

---

### Task 1: Typed service contract and failing tests

**Files:**
- Create: `tests/core/SlaveTatsServiceTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: none.
- Produces: executable expectations for `stui::core::SlaveTatsService::queryAvailable(std::string_view)` and the runtime seam.

- [ ] **Step 1: Add the CTest target**

Add an opt-in core test executable that compiles only UI-independent code:

```cmake
include(CTest)

if(BUILD_TESTING)
    add_executable(SlaveTatsUICoreTests
        tests/core/SlaveTatsServiceTests.cpp
        src/core/SlaveTatsService.cpp
    )
    target_include_directories(SlaveTatsUICoreTests PRIVATE src)
    add_test(NAME SlaveTatsUICoreTests COMMAND SlaveTatsUICoreTests)
endif()
```

- [ ] **Step 2: Write the failing service tests**

Create a `FakeTattooRuntime` implementing the wished-for interface and verify these independent behaviors with literal expectations:

```cpp
expectError(service.queryAvailable("default"), ServiceErrorCode::slaveTatsUnavailable);
expectError(service.queryAvailable("default"), ServiceErrorCode::jContainersUnavailable);
expectEntries(service.queryAvailable("custom"), {
    TattooEntry{
        .runtimeHandle = 42,
        .domain = "custom",
        .section = "Roses",
        .name = "Red Rose",
        .texturePath = "roses/red.dds",
        .area = "Body",
        .slot = -1,
        .color = 0xFFFFFF,
        .locked = false,
        .alpha = 1.0F,
    },
});
```

The fake must record the requested domain so the successful test also verifies that `"custom"` crossed the service seam unchanged.

- [ ] **Step 3: Run the test target and verify RED**

```powershell
cmake --preset build-debug -DBUILD_TESTING=ON
cmake --build build/debug --target SlaveTatsUICoreTests
```

Expected: compilation fails because `core/TattooModels.h`, `core/ITattooRuntime.h`, and `core/SlaveTatsService.h` do not exist yet. A missing compiler/toolchain error is not an acceptable RED result and must be fixed before continuing.

---

### Task 2: Implement the UI-independent service

**Files:**
- Create: `src/core/TattooModels.h`
- Create: `src/core/ITattooRuntime.h`
- Create: `src/core/SlaveTatsService.h`
- Create: `src/core/SlaveTatsService.cpp`
- Test: `tests/core/SlaveTatsServiceTests.cpp`

**Interfaces:**
- Consumes: `ITattooRuntime::apiAvailable()`, `jContainersReady()`, and `queryAvailable(std::string_view)`.
- Produces: `TattooQueryResult SlaveTatsService::queryAvailable(std::string_view domain)`.

- [ ] **Step 1: Add transport-independent value and result types**

Define:

```cpp
namespace stui::core {

struct TattooEntry {
    std::int32_t runtimeHandle{0};
    std::string sourceId;
    std::string sourceFile;
    std::string packName;
    std::string domain{"default"};
    std::string section;
    std::string name;
    std::string texturePath;
    std::string area;
    std::int32_t slot{-1};
    std::int32_t color{0xFFFFFF};
    bool locked{false};
    float alpha{1.0F};

    bool operator==(const TattooEntry&) const = default;
};

enum class ServiceErrorCode {
    slaveTatsUnavailable,
    jContainersUnavailable,
    queryAvailableFailed,
};

struct ServiceError {
    ServiceErrorCode code;
    std::string message;
};

using TattooQueryResult = std::expected<std::vector<TattooEntry>, ServiceError>;

}  // namespace stui::core
```

`runtimeHandle` is copied only to preserve the legacy Prisma payload. It must not become repository identity because available-query pools are cleaned before the result returns to the UI.

- [ ] **Step 2: Add the runtime seam**

Define an interface containing exactly the operations required by this tracer slice:

```cpp
class ITattooRuntime {
public:
    virtual ~ITattooRuntime() = default;
    [[nodiscard]] virtual bool apiAvailable() const noexcept = 0;
    [[nodiscard]] virtual bool jContainersReady() const noexcept = 0;
    virtual TattooQueryResult queryAvailable(std::string_view domain) = 0;
};
```

- [ ] **Step 3: Implement availability validation and forwarding**

`SlaveTatsService` receives `ITattooRuntime&` in its constructor. It returns these exact errors before delegating:

```cpp
ServiceError{ServiceErrorCode::slaveTatsUnavailable, "SlaveTatsNG not available"}
ServiceError{ServiceErrorCode::jContainersUnavailable, "JContainers not ready"}
```

- [ ] **Step 4: Run the focused test and verify GREEN**

```powershell
cmake --build build/debug --target SlaveTatsUICoreTests
ctest --test-dir build/debug --output-on-failure -R SlaveTatsUICoreTests
```

Expected: the core test executable builds and all service cases pass.

- [ ] **Step 5: Proposed commit checkpoint**

```text
refactor: add typed SlaveTats core service contract
```

Do not execute the commit until the user explicitly approves it.

---

### Task 3: Production SlaveTats/JContainers runtime adapter

**Files:**
- Create: `src/runtime/SlaveTatsRuntime.h`
- Create: `src/runtime/SlaveTatsRuntime.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: SlaveTatsNG `Addresses`, JContainers root/reflection/domain interfaces, and `jcmini` accessors.
- Produces: `ITattooRuntime`, lifecycle binding methods, `apiVersion()`, and temporary `api()` access for legacy `Bridge` handlers.

- [ ] **Step 1: Declare lifecycle and compatibility accessors**

```cpp
class SlaveTatsRuntime final : public core::ITattooRuntime {
public:
    void bindSlaveTats(const slavetats::interface::Addresses* api) noexcept;
    void noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept;
    bool bindJContainers(const jc::root_interface* root);

    [[nodiscard]] bool apiAvailable() const noexcept override;
    [[nodiscard]] bool jContainersReady() const noexcept override;
    [[nodiscard]] std::uint32_t apiVersion() const noexcept;
    [[nodiscard]] const slavetats::interface::Addresses* api() const noexcept;
    core::TattooQueryResult queryAvailable(std::string_view domain) override;
};
```

The `api()` accessor is explicitly transitional and only supports legacy handlers that will move in later Phase 1 slices.

- [ ] **Step 2: Implement scoped JContainer pool cleanup**

Use a local RAII guard in `SlaveTatsRuntime.cpp` so every return path calls `jcmini::JValue::cleanPool(poolName)` exactly once after a pool is created.

- [ ] **Step 3: Copy pooled records into values**

For each returned handle, copy `name`, `section`, `area`, `texture`, `slot`, `color`, `locked`, and `alpha`. Normalize legacy color `0` to `0xFFFFFF`, preserve the requested domain, and leave source fields empty until Phase 2 establishes a reliable source identity.

- [ ] **Step 4: Preserve query failure semantics**

If `query_available_tattoos` returns failure, return:

```cpp
ServiceError{ServiceErrorCode::queryAvailableFailed, "query_available_tattoos failed"}
```

- [ ] **Step 5: Compile the plugin target**

```powershell
cmake --build build/debug --target SlaveTatsUI
```

Expected: the runtime adapter compiles into the plugin without including PrismaUI headers from `src/core`.

---

### Task 4: Convert Bridge into the Prisma adapter for available queries

**Files:**
- Modify: `src/Bridge.h`
- Modify: `src/Bridge.cpp`
- Test: `tests/core/SlaveTatsServiceTests.cpp`

**Interfaces:**
- Consumes: `SlaveTatsService::queryAvailable()` and `TattooEntry`.
- Produces: the unchanged Prisma `{"type":"available","domain":...,"tattoos":[...]}` payload.

- [ ] **Step 1: Own runtime and service in Bridge**

Declare members in dependency order:

```cpp
runtime::SlaveTatsRuntime m_runtime;
core::SlaveTatsService m_service{m_runtime};
```

Remove `m_tattooAPI`, `m_jcReady`, and `m_slaveTatsAPIVersion`. Lifecycle callbacks forward to `m_runtime`; legacy handlers use the transitional `m_runtime.api()` and `m_runtime.jContainersReady()` accessors.

- [ ] **Step 2: Add typed Prisma serialization**

Add `tattooToJSON(const core::TattooEntry&)`. Preserve the current field names and types:

```json
{
  "handle": 42,
  "name": "Red Rose",
  "section": "Roses",
  "area": "Body",
  "texture": "roses/red.dds",
  "slot": -1,
  "color": 16777215,
  "locked": 0,
  "alpha": 1.00
}
```

Keep the legacy handle-based serializer for applied/slot handlers until their own Phase 1 slices migrate.

- [ ] **Step 3: Replace `handleQueryAvailable` implementation**

Call `m_service.queryAvailable(domain)`, map `ServiceError::message` to the existing Prisma error payload, serialize the returned vector, and avoid all direct SlaveTatsNG/JContainers calls in this handler.

- [ ] **Step 4: Add payload regression coverage**

Move typed Prisma serialization into a small static/private-free adapter function that the core test target can compile without PrismaUI. Add a literal JSON assertion proving special characters, default color, lock conversion, and alpha remain compatible.

- [ ] **Step 5: Run focused and existing tests**

```powershell
cmake --build build/debug --target SlaveTatsUICoreTests SlaveTatsUI
ctest --test-dir build/debug --output-on-failure
node --test tests/*.test.cjs
```

Expected: core tests pass, the SKSE plugin builds, and all existing browser pagination/structure tests pass.

- [ ] **Step 6: Verify the core dependency rule**

```powershell
rg -n "PrismaUI_API|PrismaView|IVPrismaUI" src/core src/runtime
```

Expected: no matches.

- [ ] **Step 7: Proposed commit checkpoint**

```text
refactor: extract available tattoo query from Prisma bridge
```

Do not execute the commit until the user explicitly approves it.

---

## Self-Review

- Spec coverage: this plan implements the first vertical slice of Phase 1 only; actor, applied tattoo, slot, mutation, texture, repository indexing, and native menu work remain deliberately split into later plans.
- Placeholder scan: no implementation placeholders are present; source metadata is explicitly empty in this phase because the upstream runtime contract does not expose reliable source identity.
- Type consistency: `TattooQueryResult`, `TattooEntry`, `ServiceError`, and `ITattooRuntime` names and signatures are consistent across all tasks.
- Regression boundary: Prisma payload preservation is tested at the transport adapter; core service tests exercise real orchestration through a fake runtime adapter rather than asserting mock call counts.
