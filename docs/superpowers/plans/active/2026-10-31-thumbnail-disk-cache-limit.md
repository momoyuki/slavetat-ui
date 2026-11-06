# Thumbnail Disk Cache Limit Implementation Plan

> **For agentic workers:** Use executing-plans to implement this approved plan inline, without commits.

**Goal:** Add a persistent bounded disk cache limit with safe scheduled pruning.

**Architecture:** A dedicated runtime setting owns JSON validation and persistence.
NativeThumbnailRuntime schedules maintenance, while D3D11NativeThumbnailSource
serializes disk changes through its existing mutex. The adapter only submits intent.

**Tech Stack:** Existing C++23, CMake, JSON, SKSE scheduler and standalone CTest tests.

**Spec:** `docs/superpowers/specs/2026-10-31-thumbnail-disk-cache-limit-design.md`

## Global Constraints

- Choices 128/256/512/1024 MiB; default 256; GPU budget stays 16 MiB.
- No new dependencies, commits, pushes or deployment. Preserve existing UI/hotkey edits.
- Disk deletion and config persistence run outside render; no actor-runtime changes.

## Review Focus

- JSON booleans/floats/overflow/unknown integers fall back to default.
- Failed saves preserve prior config and applied budget.
- Shrink failure exposes failure and permits retry without a stuck pending state.
- Build/clear/limit requests cannot overlap; scheduler exceptions release the guard.
- Startup applies saved limits before launcher/browser work.

### Task 1: Persistent setting

**Files:** Create `src/runtime/ThumbnailCacheSetting.h/.cpp`,
`tests/runtime/ThumbnailCacheSettingTests.cpp`; modify `CMakeLists.txt`.
**Interfaces:** `load()`, `select(uint32_t)`, `limitMiB()`, static `valid(uint32_t)`;
consume shared PluginConfigFile. Atomic value supports worker/UI access.

- [x] Write tests for default, every choice, reset, invalid types/ranges, preserve
  unrelated keys and failed save. Build to demonstrate missing implementation.
- [x] Implement the dedicated setting and run ThumbnailCacheSettingTests.

### Task 2: Safe source/runtime maintenance

**Files:** Modify `ThumbnailDiskCache.h/.cpp`, `D3D11NativeThumbnailSource.h/.cpp`,
`NativeThumbnailRuntime.h/.cpp`, corresponding native tests.
**Interfaces:** `ThumbnailDiskCache::setMaxBytes(uintmax_t) -> bool`;
source `setDiskCacheLimit(uintmax_t) -> bool`;
runtime `requestDiskCacheLimit(uint32_t, function<bool(uint32_t)>) -> bool`,
`diskCacheLimitState()`, `diskCacheLimitMiB()`.

- [x] Write failing tests for shrink eviction, preservation of non-cache files,
  deferred execution, conflicting operations, save/source/scheduler failure and retry.
- [x] Implement failure-reporting prune and source mutex wrapper; schedule apply and
  persist with budget rollback on save failure. Run native thumbnail tests.

### Task 3: Launcher/startup and verification

**Files:** Modify `main.cpp`, `OfficialMenuFrameworkAdapter.h/.cpp`, adapter tests,
thumbnail grid spec and `ROADMAP.md`.
**Interfaces:** Add ThumbnailCacheSetting parameter to renderLauncher; startup loads
and schedules stored limit. Launcher displays discrete choices, reset, used/limit,
pending/failure and respects runtime guards.

- [x] Build the adapter test target with the setting interface; scheduled-intent
  coverage lives in runtime tests. Native ImGui interaction needs the in-game smoke test.
- [x] Wire UI/startup without config writes or deletion in render.
- [x] Build Debug then Release and run each full CTest suite sequentially; run
  `git diff --check`, inspect focused diff and record manual QA limitations.

## Verification record

- Config tests passed with all discrete choices, missing/invalid fallback, integer
  overflow, reset, failed-save state, and preservation of unrelated settings.
- Runtime tests first failed compilation on missing limit APIs before implementation.
  The rollback failure test then reproduced the incorrect applied-budget display
  before the fix.
- Source mutex regression failed before moving disk lookup inside the source mutex;
  all 20 focused source cases then passed, including immediate shrink and filesystem
  failure reporting. Latest full builds will include the added GPU-retention assertion.
- Dependency restoration used only locally cached source archives. The existing
  CommonLib overlay reported its pre-existing release-only OpenVR binary warning.
- Debug full build passed; sequential full CTest passed 56/56 (5.70 seconds),
  including latest rollback, source mutex, GPU retention, and setting assertions.
- Release (RelWithDebInfo) full build passed; sequential full CTest passed 56/56
  (3.51 seconds). Neither configuration skipped tests. `git diff --check` passed.
- No standalone C++ lint target is configured; both builds perform compiler type
  checking. Manual in-game launcher verification remains pending; no commit or deployment.

- [ ] In-game smoke test: choose 128/512/1024, shrink a populated cache, reset to
  256, restart to verify persistence, and confirm build/clear are unavailable while
  the limit is pending. Keep this plan active until that acceptance is recorded.
