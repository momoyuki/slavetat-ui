# Native Menu Foundation Design

## Context

SlaveTatsUI is migrating from a PrismaUI frontend to a native SKSE Menu Framework frontend according to `SKSE_MENU_MIGRATION.md`. The UI-independent tattoo repository, loose/BSA texture resolution, DDS decoding, D3D11 upload, and bounded GPU cache already exist. Runtime validation confirmed that Skyrim exposes a usable `ID3D11Device` during `kDataLoaded`, and the current Prisma workflow remains functional.

The installed target is SKSE Menu Framework 3.13 Hotfix 2. Its official v3 API dynamically resolves exports from `SKSEMenuFramework.dll` and provides `AddSectionItem` and `AddWindow` entry points. The first native slice must validate that integration without weakening the working Prisma frontend.

## Goals

- Register a `SlaveTatsUI` section and `Tattoo Browser` entry in the SKSE Menu Framework main menu.
- Open a non-pausing native side-panel window from that entry.
- Keep the existing F8 PrismaUI workflow unchanged during native development.
- Treat SKSE Menu Framework as an optional runtime dependency until Prisma retirement.
- Isolate framework and ImGui API types from core, repository, runtime, and texture modules.
- Make registration, fallback, duplicate-registration prevention, and window state testable without Skyrim.
- Establish a safe foundation for repository paging and GPU thumbnails in later slices.

## Non-goals

- Do not browse, apply, remove, recolor, or synchronize tattoos in the first slice.
- Do not decode or upload DDS textures from the render callback.
- Do not remove PrismaUI, Base64 transport, or the `.rgba` thumbnail cache yet.
- Do not change the F8 hotkey behavior.
- Do not add custom D3D11 or input hooks; SKSE Menu Framework owns rendering and menu input.
- Do not implement the full Phase 5 native menu in one change.

## Migration Decision

PrismaUI will be retired only after the native frontend can complete the common workflow in game:

1. browse and paginate the catalog,
2. display loose and BSA-backed thumbnails,
3. inspect actor slots,
4. apply and remove tattoos,
5. edit color and alpha,
6. synchronize changes without crashes.

After those checks pass, a dedicated retirement change will switch F8 to the native menu, stop creating the Prisma view, and remove Prisma-only texture transport. PrismaUI may remain installed in MO2 for unrelated mods.

## Architecture

```text
SKSEPluginLoad
    |
    +-- existing Prisma lifecycle
    |
    +-- NativeMenu::registerMenu(MenuFrameworkPort&)
            |
            +-- OfficialMenuFrameworkAdapter
            |       |
            |       +-- SKSE Menu Framework v3 dynamic exports
            |
            +-- Tattoo Browser section callback
            +-- non-pausing side-panel render callback
```

### NativeMenu

`NativeMenu` owns only native-window registration and presentation state. It exposes an idempotent registration operation and static ABI-safe callbacks that forward to its single process-lifetime instance. It does not include `Bridge`, PrismaUI, JContainers, texture resolution, or SlaveTatsNG APIs.

The section callback opens the registered window. The render callback positions the window on the right side of the main viewport at approximately 40 percent width and renders foundation status content. The first slice performs no external I/O during rendering.

### MenuFrameworkPort

`MenuFrameworkPort` is the narrow application-owned boundary used by `NativeMenu`. It represents only the operations required by this project:

- report runtime availability and version,
- set the owning section,
- register a section item,
- register a non-pausing window,
- open and close that window safely.

Tests use a fake port. Core modules never depend on the official header or ImGui types.

### OfficialMenuFrameworkAdapter

The production adapter is the only module that includes the official SKSE Menu Framework header. It translates the framework's dynamic API and `WindowInterface` into `MenuFrameworkPort` behavior. Missing DLLs, missing exports, unsupported runtime versions, and null window handles are reported as ordinary registration failures rather than process failures.

## Official API Provenance

The v3 client header will be vendored from `QTR-Modding/SKSE-Menu-Framework-3` at revision `3a65dc0147388da177c324cff4d89d9e25094623`. The official example used to validate registration and lifecycle is `QTR-Modding/SKSE-Menu-Framework-3-Example` at revision `974e82a094b16c4e5469a0d2189b2caff3f9742a`.

The vendored file must retain its upstream attribution, revision, and LGPL-2.1 license notice. Builds must not download the header or framework from the network. Runtime loading remains dynamic; SlaveTatsUI does not link directly to `SKSEMenuFramework.dll`.

## Lifecycle and Data Flow

1. `SKSEPluginLoad` initializes the existing plugin services.
2. The composition root asks `NativeMenu` to register through `OfficialMenuFrameworkAdapter`.
3. If the framework is available and compatible, the adapter registers the section item and non-pausing window exactly once.
4. Selecting `SlaveTatsUI/Tattoo Browser` sets the window's open state.
5. SKSE Menu Framework invokes the render callback while the window is open.
6. The callback renders only local presentation state in the first slice.
7. F8 continues to call `Bridge::toggleUI()` and therefore opens PrismaUI.

Later slices will inject immutable catalog snapshots into the native presentation model. Filtering and pagination will occur before texture work. Only entries on the current page may request DDS bytes and `D3D11TextureManager` resources.

## Window Behavior

- The native window is non-pausing.
- Its initial position is anchored to the right edge with a small margin.
- Its initial width is 40 percent of the usable viewport, leaving the actor visible.
- The user may resize or move it after first appearance.
- Closing the window changes presentation state only; repositories and GPU caches remain alive.
- Reopening the window must not repeat registration or create duplicate menu items.

## Error Handling

- Missing `SKSEMenuFramework.dll`: log one warning, leave native UI unavailable, and preserve PrismaUI.
- Unsupported framework version: log the detected version and required major version, then preserve PrismaUI.
- Missing required export or failed section registration: return a named registration error and do not create a partially usable menu.
- Null window handle: keep the menu unavailable and never dereference it.
- Repeated registration: return the existing registration state without adding duplicate entries.
- Render exception: catch it inside the callback boundary, log it, and render a safe error message when the framework remains usable. No exception may cross the framework ABI.
- Missing D3D11 device, repository, or texture: irrelevant to the first render slice and must not prevent menu registration.

## Testing

### Automated

- Available framework registers one section item and one non-pausing window.
- Unavailable framework performs no registrations and returns unavailable status.
- Unsupported versions fail without partial registration.
- Null window registration results are rejected safely.
- Repeated registration does not duplicate the section item or window.
- Invoking the section callback opens the registered window.
- Render failures are contained at the ABI boundary.
- Complete Debug and Release builds and CTest suites pass.
- Formatting, whitespace, machine-specific path, debug-marker, and secret scans pass.

### In-game smoke test

1. Open the framework with F1 and locate `SlaveTatsUI/Tattoo Browser`.
2. Open the native side panel and confirm that the actor remains visible and the game does not pause.
3. Close and reopen the panel repeatedly without a crash or duplicate entry.
4. Press F8 and confirm that PrismaUI still opens.
5. Use PrismaUI to browse and apply/remove one tattoo as a regression check.
6. Confirm the log contains successful native registration and no missing-export, renderer, or callback errors.
7. Optionally disable SKSE Menu Framework in MO2 and confirm that SlaveTatsUI and PrismaUI still load.

## Follow-up Slices

1. Inject catalog snapshots and implement read-only search, filters, and pagination.
2. Resolve and upload current-page thumbnails through `TextureResolver` and `D3D11TextureManager`.
3. Move actor, slot, and mutation operations behind shared core services and add native controls.
4. Complete the native in-game acceptance checklist.
5. Retire PrismaUI transport and switch F8 in a dedicated reversible change.

## Acceptance Criteria

- A native `SlaveTatsUI/Tattoo Browser` entry appears under F1 when SKSE Menu Framework 3.x is available.
- The entry opens one non-pausing right-side window and does not duplicate on repeated registration.
- Missing or incompatible framework installations do not prevent plugin load or PrismaUI use.
- F8 and all existing Prisma workflows remain unchanged.
- No core, repository, runtime, or texture header depends on SKSE Menu Framework or ImGui.
- Automated and in-game checks described above pass before the implementation commit is proposed.
