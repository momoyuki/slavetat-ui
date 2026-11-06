# Browser Preference Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Retain Thumbnail BG and safe last Player Browser views across restarts.

**Architecture:** Shared PluginConfigFile preserves unrelated settings. A bounded, thread-safe Player view store owns serialized preferences and generation protection; the native camera port alone captures rendered views and restores game ownership.

**Tech Stack:** Existing C++23, nlohmann JSON, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-31-browser-preference-persistence-design.md`

## Global Constraints

No new dependencies, commits, push, deployment, NPC camera control, or tattoo runtime changes. World-axis translation-relative view behavior is explicit. No native pointers or absolute world positions are persisted.

## Review Focus

- Clear while an old session is pending must prevent re-saving the cleared view.
- Load/newgame must discard live state without overwriting saved preferences.
- One malformed side must not poison the other side.
- Config writes must preserve BG/cache/hotkey/favorites and framing settings.
- Failed persistence must never prevent exact normal-camera restoration.

### Task 1: Preference settings and validation

Files: new runtime ThumbnailBackgroundSetting and native BrowserCameraPreferenceStore headers/sources/tests; BrowserCameraPose; CMakeLists.txt.

- [x] Add regression tests for BG restart/rollback, camera roundtrip/bounds/malformed side/generation/rollback, malformed camera arrays.
- [x] Build/run targeted tests and record expected red failures before production changes.
- [x] Implement the two independent stores; `BrowserCameraPreferenceStore::remember(bool, pose, generation)` retains a valid session view on write failure, while `clear()` rolls back on failure and increments generation on success.
- [x] Run targeted tests and record green.

### Task 2: Camera lifecycle and UI integration

Files: BrowserCameraSession/Coordinator, SkyrimBrowserCameraPort, main.cpp, OfficialMenuFrameworkAdapter and tests.

- [x] Test invalidation restores ownership without normal close capture.
- [x] Watch expected failure, then route lifecycle reset through discard/invalidation and attach loaded store during initialization.
- [x] Use the store for Player recall/capture, logging failures and preserving restoration.
- [x] Inject BG setting into renderFoundation and a clear callback into renderLauncher. Show actionable success/failure feedback; retain existing Reset View meaning.
- [x] Verify targeted tests and scoped diff.

### Task 3: Documentation and complete verification

- [x] Update roadmap and previous specs to explicitly supersede session-only persistence.
- [x] Announce review-ready implementation before full builds.
- [x] Run complete Debug build/CTest, then complete Release build/CTest; run git diff --check and inspect scoped diff.
- [x] Report results and defer only combined in-game acceptance, without commit/push/deploy.

- [ ] User combined in-game acceptance (deferred by user; keeps this plan active).

## Execution record

- RED: existing pose tests failed on unbounded saved offsets; new store failed to compile before its header existed; session invalidation regression failed on missing discard/invalidate methods.
- GREEN: focused Debug tests passed 6/6 after implementation.
- Review fix RED: queued-clear regression failed on missing clearSavedViews method. GREEN: clear invalidates coordinator intent before synchronous preference removal; the regression forces old queued opening execution during clear and observes no camera movement.
- Full Debug build and CTest: 58/58 passed, zero skipped.
- Full Release build and CTest: 58/58 passed, zero skipped.
- Focused independent review: queued-clear finding resolved; no remaining Important/Critical findings.
- Ruling: retain existing world-axis translation-relative orientation, so turning Player does not rotate the recalled view; changing this later requires a separate framing decision.
- Ruling: avoid a global configuration file size cap because that would change unrelated favorites/loadout behavior. Validate bounded camera subtree shape and values instead.
- Ruling: a failed Reset still invalidates live/queued camera safely, while retaining stored preferences and displaying failure. This prevents stale capture races without claiming a successful disk change.
- Combined in-game acceptance remains pending by user choice. No commit, push, or deployment performed.
