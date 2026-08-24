# SKSE Menu Framework Migration Plan

## Goal

Move SlaveTatsUI toward a native SKSE Menu Framework frontend while preserving the current SlaveTatsNG integration and PrismaUI behavior during migration.

The migration should reduce UI/texture transport complexity without rewriting the tattoo-management core, while also improving the everyday workflow for large SlaveTats libraries.

The native UI should be designed around four product goals:

1. keep the selected actor visible while editing tattoos,
2. remain responsive with 800+ and preferably several thousand tattoo definitions,
3. provide visual thumbnail browsing without pre-generating the whole library,
4. expose meaningful grouping from the SlaveTats JSON source, pack, section, and area metadata.

## Why migrate

The current PrismaUI path couples several concerns inside `Bridge`:

- PrismaUI view lifecycle and JSON command dispatch,
- SlaveTatsNG API access,
- actor/slot/tattoo queries and mutation logic,
- JContainers integration,
- DDS loading and BSA resource access,
- thumbnail conversion, disk caching, and RGBA/Base64 transport to the browser.

That design works, but it makes the frontend expensive to maintain and forces DDS textures through a browser-oriented thumbnail pipeline.

A native SKSE Menu Framework frontend can call C++ services directly and render GPU textures with ImGui. This removes the need for JSON/Base64 texture transport and can avoid persistent generated thumbnail files.

## Non-goals

- Do not remove PrismaUI in the first implementation PR.
- Do not change SlaveTatsNG behavior or public APIs.
- Do not rewrite actor/slot semantics.
- Do not load every tattoo texture eagerly.
- Do not rely on loose-file-only texture loading; BSA assets must remain supported.
- Do not build a fullscreen UI that unnecessarily hides the actor being edited.
- Do not use infinite scrolling as the only navigation method for very large tattoo libraries.
- Do not infer a mod/pack name solely from `section`; preserve source JSON identity separately.

## UX requirements

### Actor-visible layout

The primary workflow is visual: select a tattoo, apply it, and immediately inspect the result on the actor.

The native menu should therefore default to a side-panel layout rather than a centered/fullscreen window.

Recommended behavior:

- dock/anchor the browser to the left or right side of the screen,
- target roughly 35-45% of the usable viewport width by default,
- leave the center/opposite side unobstructed for the actor preview,
- remember the user's last side and size where practical,
- allow resizing without requiring the UI to fill the screen,
- avoid modal dialogs for common apply/remove/edit operations.

Conceptual layout:

```text
+--------------------------+--------------------------------------+
| SlaveTats Browser        |                                      |
| Search / Filters         |                                      |
|                          |           ACTOR PREVIEW              |
| [thumb] [thumb] [thumb]  |                                      |
| [thumb] [thumb] [thumb]  |                                      |
|                          |                                      |
| < Prev   Page 3/28 Next >|                                      |
+--------------------------+--------------------------------------+
```

The UI may later support alternate layouts, but the actor-visible side panel should be the baseline design.

### Pagination, not scroll-only navigation

For large libraries, pagination should be the default browsing model.

Recommended defaults:

- 24 entries per page initially,
- allow a later user preference such as 12 / 24 / 48 entries,
- show current page and total page count,
- provide Previous / Next controls,
- reset to page 1 after a filter change that invalidates the current page,
- keep keyboard/controller-friendly navigation possible.

Example:

```text
< Previous        Page 7 / 34        Next >
```

Scrolling may still be used inside individual panels, but the tattoo library itself should not require scrolling through hundreds or thousands of cards.

### Search and filters

The browser should support combinable filters:

- text search,
- JSON source / tattoo pack,
- section,
- area (BODY / FACE / HANDS / FEET and any future area values),
- optional sort order.

The UI should show the filtered count, for example:

```text
Showing 37 of 1,842 tattoos
```

Search/filter operations should operate on in-memory metadata and must not require DDS decoding.

## Tattoo metadata and JSON source model

SlaveTats tattoo definitions originate from JSON-backed data. The native browser should preserve enough provenance to group tattoos by their source instead of flattening everything into one list.

A tattoo entry should conceptually retain:

```cpp
struct TattooEntry {
    std::string sourceId;      // stable/internal source identity where available
    std::string sourceFile;    // JSON source path/name
    std::string packName;      // display name derived from source metadata/file
    std::string section;
    std::string name;
    std::string texturePath;
    std::string area;
};
```

Important distinction:

- `packName` / `sourceFile` identifies which SlaveTats JSON source the tattoo came from,
- `section` remains the section defined by the tattoo data,
- `area` remains the body area used by SlaveTats.

Do not assume `section == mod name`.

Suggested hierarchy:

```text
JSON source / Pack
  -> Section
      -> Area
          -> Tattoo
```

The exact source-name derivation should be reviewed against the actual SlaveTatsNG/JContainers data available at runtime. If the runtime API does not expose the JSON filename directly, the repository layer should define a deterministic fallback rather than silently merging unrelated packs.

## Target architecture

```text
                         SlaveTatsNG
                              |
                              v
                     SlaveTatsService
                       /           \
                      /             \
             Actor / Slots      Tattoo queries
                      \             /
                       \           /
                     TattooRepository
                         /        \
                        /          \
                 source metadata   filters/indexes
                        \          /
                         \        /
                       TextureResolver
                         /        \
                        /          \
                 loose files     BSResource/BSA
                        \          /
                         \        /
                         TextureManager
                              |
                     bounded D3D11 SRV cache
                              |
                  +-----------+-----------+
                  |                       |
              PrismaUI               SKSE Menu
          (temporary legacy)            ImGui
```

## Migration phases

### Phase 1 — Extract UI-independent services

Refactor `Bridge` without changing user-visible behavior.

Create UI-independent components for:

- SlaveTatsNG API binding and availability checks,
- actor discovery,
- available/applied tattoo queries,
- BODY/FACE/HANDS/FEET slot queries,
- apply/remove/update/synchronize operations,
- conversion of JContainers tattoo data into C++ models.

Suggested files:

```text
src/core/SlaveTatsService.h
src/core/SlaveTatsService.cpp
src/core/TattooModels.h
src/core/TattooRepository.h
src/core/TattooRepository.cpp
```

`Bridge` should become a thin Prisma transport adapter that:

1. parses incoming JSON,
2. dispatches work to the service,
3. serializes service results back to JSON.

Acceptance criteria:

- PrismaUI still behaves the same as before,
- existing actor, slot, apply, remove, update, and synchronize flows still work,
- core service headers do not include `PrismaUI_API.h`,
- tattoo metadata is represented independently of JSON transport.

### Phase 2 — Build tattoo repository and indexes

Create an in-memory repository optimized for browse/search/filter operations.

Responsibilities:

- load/receive tattoo metadata once per refresh,
- retain source JSON / pack identity where available,
- index by source/pack, section, area, and normalized searchable name,
- return filtered result sets without decoding textures,
- expose stable ordering for pagination.

Suggested files:

```text
src/core/TattooRepository.h
src/core/TattooRepository.cpp
src/core/TattooModels.h
```

Acceptance criteria:

- an 800+ entry library can be filtered without texture I/O,
- source/pack and section remain separate fields,
- changing filters does not re-query or re-decode every tattoo texture,
- pagination receives a deterministic result ordering.

### Phase 3 — Extract texture resolution

Separate texture discovery/loading from browser serialization.

Suggested files:

```text
src/textures/TextureResolver.h
src/textures/TextureResolver.cpp
```

Responsibilities:

- normalize SlaveTats texture paths,
- resolve loose files,
- resolve BSA-backed resources through Skyrim/BSResource,
- return DDS bytes or a resource object independent of PrismaUI.

Keep the existing resource-loading knowledge already implemented in the project rather than replacing it with manual BSA parsing.

Acceptance criteria:

- loose and BSA textures still resolve,
- no PrismaUI types are exposed by `TextureResolver`,
- texture resolution can be reused by a future native renderer.

### Phase 4 — Add native GPU texture manager

Create a runtime D3D11 texture cache for the native UI.

Suggested files:

```text
src/textures/TextureManager.h
src/textures/TextureManager.cpp
```

Responsibilities:

- decode DDS data from `TextureResolver`,
- create `ID3D11ShaderResourceView` objects,
- cache textures by normalized texture path,
- expose width/height metadata,
- release resources when appropriate,
- fail gracefully for unsupported/broken textures,
- enforce a bounded cache rather than keeping every viewed tattoo resident forever.

The native path should not write generated `.rgba` thumbnails to disk.

Recommended cache behavior:

- lazy load on first visible use,
- retain recently viewed textures for fast page-back navigation,
- evict least-recently-used entries when over the cache budget,
- make the cache budget configurable later if real-world packs require it,
- never load all 800+ textures just because the menu opened.

Acceptance criteria:

- a resolved DDS can be rendered through ImGui,
- cache hits do not decode/upload the texture again,
- texture ownership is explicit and leak-free,
- cache memory remains bounded while navigating a large library.

### Phase 5 — Add SKSE Menu Framework frontend

Add an ImGui frontend alongside PrismaUI.

Suggested files:

```text
src/ui/Menu.h
src/ui/Menu.cpp
src/ui/ActorSelector.cpp
src/ui/SlotView.cpp
src/ui/TattooBrowser.cpp
src/ui/TattooEditor.cpp
```

Initial feature set:

- actor selector,
- BODY/FACE/HANDS/FEET tabs,
- current slot state,
- text search,
- source/pack filter,
- section filter,
- area filter,
- thumbnail grid,
- pagination,
- apply/replace/remove,
- color picker,
- alpha control,
- synchronize action,
- side-panel layout that leaves the actor visible.

Do not attempt feature-complete visual parity with the web UI in the first native PR.

Acceptance criteria:

- the native menu can complete the common workflow without opening MCM or PrismaUI,
- current tattoo state is read from the same core service as PrismaUI,
- apply/remove/update actions share the same service implementation,
- the default menu layout does not unnecessarily cover the actor,
- the browser exposes thumbnail, source/pack, section, area, and search navigation.

### Phase 6 — Pagination and lazy texture loading

Large tattoo packs can contain hundreds or thousands of textures. The native UI must avoid eager loading.

Baseline strategy:

- filter/sort metadata first,
- paginate the resulting metadata list,
- request textures only for the current page (and optionally prefetch the next/previous page),
- keep a bounded runtime LRU cache,
- use a placeholder while a texture is unavailable or still being prepared,
- never decode all filtered results solely to compute the page.

Recommended initial setting:

```text
24 tattoos / page
```

Examples:

```text
800 tattoos   ~= 34 pages
2,000 tattoos ~= 84 pages
```

Only the current page should require immediate thumbnail availability.

Acceptance criteria:

- opening the menu does not upload the full tattoo library to the GPU,
- changing page remains responsive with 800+ entries,
- search/filter does not perform DDS decoding,
- memory usage stays bounded,
- returning to a recently viewed page normally hits the runtime cache.

### Phase 7 — Retire Prisma-specific thumbnail transport

Only after the native UI is stable:

Remove or deprecate:

- `.rgba` thumbnail generation/cache,
- Base64 texture transport,
- texture JSON payloads,
- PrismaUI-specific texture decode code.

Keep PrismaUI itself temporarily if users still need it, or remove it in a dedicated compatibility-breaking PR/release.

## Thumbnail policy

The native browser should have thumbnails by default.

Thumbnail requirements:

- render from the actual SlaveTats texture source,
- support both loose and BSA-backed DDS assets,
- avoid persistent generated thumbnail files in the native path,
- preserve aspect ratio where practical,
- use a safe placeholder for missing/broken textures,
- avoid decoding off-page tattoos,
- avoid duplicate GPU uploads for the same normalized texture path.

A future optimization may create smaller runtime GPU previews rather than uploading full-resolution tattoo textures if profiling shows that full DDS uploads consume excessive VRAM. This should be driven by measurement rather than assumed up front.

## Performance targets

The design target is not merely "works with 800 tattoos"; large libraries should remain pleasant to browse.

Initial targets to validate during implementation:

- 800+ tattoo metadata entries should not cause noticeable UI hitching during search/filter,
- opening the browser should not synchronously decode the entire tattoo library,
- changing pages should only initiate work for page-local thumbnails,
- no repeated SlaveTats/JContainers full-library query should occur every ImGui frame,
- no repeated DDS decode/upload should occur every ImGui frame,
- GPU texture residency should remain bounded by cache policy,
- menu close/reopen should not leak SRVs or worker resources.

All expensive discovery work should be event-driven (startup, data refresh, actor change where required), not frame-driven.

## Suggested PR sequence

### PR 1 — Core extraction

**Title:** `Refactor UI-independent SlaveTats services out of Prisma bridge`

Scope:

- create C++ service/model layer,
- move actor/slot/tattoo operations out of `Bridge`,
- keep PrismaUI behavior unchanged.

### PR 2 — Tattoo repository and metadata indexes

**Title:** `Add tattoo repository and source-aware filtering model`

Scope:

- normalize tattoo metadata,
- retain JSON source/pack identity,
- add search/section/area/source indexes,
- establish deterministic pagination order.

### PR 3 — Texture resolver extraction

**Title:** `Extract reusable loose/BSA texture resolver`

Scope:

- move resource resolution out of browser-specific code,
- preserve current thumbnail behavior.

### PR 4 — Native texture renderer

**Title:** `Add bounded D3D11 texture cache for native UI rendering`

Scope:

- DDS bytes -> GPU SRV,
- lazy runtime cache,
- no persistent thumbnail generation for native rendering.

### PR 5 — SKSE Menu MVP

**Title:** `Add SKSE Menu Framework tattoo browser MVP`

Scope:

- actor selector,
- actor-visible side-panel layout,
- slots,
- search/source/section/area filters,
- thumbnail grid,
- pagination,
- apply/remove,
- color/alpha controls.

### PR 6 — Performance and UX

**Title:** `Tune tattoo paging, cache behavior, and native UI polish`

Scope:

- cache budgeting/LRU behavior,
- optional prefetch,
- favorites/sorting/UX improvements as appropriate,
- profiling against large real-world tattoo packs.

## Compatibility strategy

During migration, PrismaUI and SKSE Menu should use the same core services. This provides a regression reference and prevents tattoo-management logic from diverging between frontends.

The migration should favor additive changes until the native frontend is proven stable.

MCM/SlaveTatsNG public behavior is outside the scope of this frontend migration and should not be broken by the UI work.

## Testing checklist

### Core behavior

- Player actor works.
- Nearby/non-player actor selection works.
- BODY/FACE/HANDS/FEET slots match current state.
- Apply to empty slot works.
- Replace occupied slot works.
- Remove from slot works.
- Color changes persist.
- Alpha changes persist.
- Synchronize updates overlays correctly.

### Metadata / grouping

- JSON source/pack grouping is stable.
- Source/pack and section are not conflated.
- Section filtering works.
- Area filtering works.
- Text search works across expected fields.
- Combining source + section + area + search works.
- Filter changes reset/clamp invalid page indices correctly.

### Thumbnail / resources

- Loose DDS previews render.
- BSA DDS previews render.
- Missing/broken textures show a safe placeholder.
- Duplicate texture paths reuse cached GPU resources.

### Large-library behavior

- Test with at least 800 tattoo definitions.
- Test with a larger synthetic or real library if available (2,000+ preferred).
- Opening the menu does not eagerly allocate all GPU textures.
- Page changes do not freeze the game/UI.
- Search/filter does not decode thumbnails.
- Returning to a recent page benefits from cache hits.
- Cache eviction releases GPU resources.
- Closing/reopening the menu does not leak textures.

### UX

- Default layout leaves a useful portion of the actor visible.
- Common apply/edit/remove flow does not require modal dialogs.
- Current page / total pages are always understandable.
- Empty filter results are handled clearly.
- Loading/missing thumbnail states are visually distinct.

## Open design questions for review

These should be decided before the native frontend is considered stable:

1. What exact SlaveTatsNG/JContainers field can reliably identify the source JSON or pack at runtime?
2. Should `packName` be the JSON filename, explicit metadata (if available), or a user-friendly derived value?
3. What should the default page size be after testing 1080p, 1440p, and ultrawide layouts?
4. Should page-adjacent thumbnails be prefetched, or should loading remain strictly current-page only?
5. What cache budget gives good back-navigation without excessive VRAM usage?
6. Should the UI pause the game, remain non-blocking, or expose this as a setting?
7. Should actor selection include only player/target/nearby actors in the MVP, or a broader actor browser?

## Decision

Continue developing `slavetat-ui`, but treat the project as the host for a new native frontend rather than starting over.

Reuse the current SlaveTatsNG integration, slot workflow, JContainers logic, and BSA resource knowledge. Replace the Prisma-specific UI and texture transport incrementally after those concerns have been separated from the core.

The native frontend should not simply recreate the MCM/Prisma list in ImGui. It should be a purpose-built visual tattoo manager: actor-visible, thumbnail-first, source-aware, searchable, paginated, and safe for large libraries.