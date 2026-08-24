# SKSE Menu Framework Migration Plan

## Goal

Move SlaveTatsUI toward a native SKSE Menu Framework frontend while preserving the current SlaveTatsNG integration and PrismaUI behavior during migration.

The migration should reduce UI/texture transport complexity without rewriting the tattoo-management core.

## Why migrate

The current PrismaUI path couples several concerns inside `Bridge`:

- PrismaUI view lifecycle and JSON command dispatch
- SlaveTatsNG API access
- actor/slot/tattoo queries and mutation logic
- JContainers integration
- DDS loading and BSA resource access
- thumbnail conversion, disk caching, and RGBA/Base64 transport to the browser

That design works, but it makes the frontend expensive to maintain and forces DDS textures through a browser-oriented thumbnail pipeline.

A native SKSE Menu Framework frontend can call C++ services directly and render GPU textures with ImGui. This removes the need for JSON/Base64 texture transport and can avoid persistent generated thumbnail files.

## Non-goals

- Do not remove PrismaUI in the first implementation PR.
- Do not change SlaveTatsNG behavior or public APIs.
- Do not rewrite actor/slot semantics.
- Do not load every tattoo texture eagerly.
- Do not rely on loose-file-only texture loading; BSA assets must remain supported.

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
                        TextureResolver
                         /        \
                        /          \
                 loose files     BSResource/BSA
                        \          /
                         \        /
                         TextureManager
                              |
                          D3D11 SRV cache
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

- SlaveTatsNG API binding and availability checks
- actor discovery
- available/applied tattoo queries
- BODY/FACE/HANDS/FEET slot queries
- apply/remove/update/synchronize operations
- conversion of JContainers tattoo data into C++ models

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

- PrismaUI still behaves the same as before.
- Existing actor, slot, apply, remove, update, and synchronize flows still work.
- Core service headers do not include `PrismaUI_API.h`.

### Phase 2 — Extract texture resolution

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

### Phase 3 — Add native GPU texture manager

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
- fail gracefully for unsupported/broken textures.

The native path should not write generated `.rgba` thumbnails to disk.

Acceptance criteria:

- a resolved DDS can be rendered through ImGui,
- cache hits do not decode/upload the texture again,
- texture ownership is explicit and leak-free.

### Phase 4 — Add SKSE Menu Framework frontend

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
- tattoo search/filter,
- thumbnail grid,
- apply/replace/remove,
- color picker,
- alpha control,
- synchronize action.

Do not attempt feature-complete visual parity with the web UI in the first native PR.

Acceptance criteria:

- the native menu can complete the common workflow without opening MCM or PrismaUI,
- current tattoo state is read from the same core service as PrismaUI,
- apply/remove/update actions share the same service implementation.

### Phase 5 — Lazy texture loading

Large tattoo packs can contain hundreds or thousands of textures. The native UI must avoid eager loading.

Recommended strategy:

- render tattoo entries in a clipped/virtualized grid,
- request textures only for visible or near-visible cards,
- keep a bounded runtime cache,
- optionally evict least-recently-used textures when a configurable memory threshold is reached.

Acceptance criteria:

- opening the menu does not upload the full tattoo library to the GPU,
- scrolling remains responsive with large packs,
- memory usage stays bounded.

### Phase 6 — Retire Prisma-specific thumbnail transport

Only after the native UI is stable:

Remove or deprecate:

- `.rgba` thumbnail generation/cache,
- Base64 texture transport,
- texture JSON payloads,
- PrismaUI-specific texture decode code.

Keep PrismaUI itself temporarily if users still need it, or remove it in a dedicated compatibility-breaking PR/release.

## Suggested PR sequence

### PR 1 — Core extraction

**Title:** `Refactor UI-independent SlaveTats services out of Prisma bridge`

Scope:

- create C++ service/model layer,
- move actor/slot/tattoo operations out of `Bridge`,
- keep PrismaUI behavior unchanged.

### PR 2 — Texture resolver extraction

**Title:** `Extract reusable loose/BSA texture resolver`

Scope:

- move resource resolution out of browser-specific code,
- preserve current thumbnail behavior.

### PR 3 — Native texture renderer

**Title:** `Add D3D11 texture cache for native UI rendering`

Scope:

- DDS bytes -> GPU SRV,
- runtime cache,
- no persistent thumbnail generation for native rendering.

### PR 4 — SKSE Menu MVP

**Title:** `Add SKSE Menu Framework tattoo browser MVP`

Scope:

- actor selector,
- slots,
- search,
- thumbnail grid,
- apply/remove,
- color/alpha controls.

### PR 5 — Performance and UX

**Title:** `Add lazy tattoo texture loading and native UI polish`

Scope:

- clipping/virtualization,
- bounded cache,
- favorites/filtering/UX improvements as appropriate.

## Compatibility strategy

During migration, PrismaUI and SKSE Menu should use the same core services. This provides a regression reference and prevents tattoo-management logic from diverging between frontends.

The migration should favor additive changes until the native frontend is proven stable.

## Testing checklist

- Player actor works.
- Nearby/non-player actor selection works.
- BODY/FACE/HANDS/FEET slots match current state.
- Apply to empty slot works.
- Replace occupied slot works.
- Remove from slot works.
- Color changes persist.
- Alpha changes persist.
- Synchronize updates overlays correctly.
- Loose DDS previews render.
- BSA DDS previews render.
- Missing/broken textures show a safe placeholder.
- Large tattoo packs do not eagerly allocate all GPU textures.
- Closing/reopening the menu does not leak textures.

## Decision

Continue developing `slavetat-ui`, but treat the project as the host for a new native frontend rather than starting over.

Reuse the current SlaveTatsNG integration, slot workflow, JContainers logic, and BSA resource knowledge. Replace the Prisma-specific UI and texture transport incrementally after those concerns have been separated from the core.