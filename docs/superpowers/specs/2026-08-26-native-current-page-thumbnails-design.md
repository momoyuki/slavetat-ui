# Native Current-Page Thumbnails Design

## Context

The native catalog browser renders six read-only tattoo cards per page while PrismaUI remains available through F8. `TextureResolver` already resolves loose files before BSA resources, and `D3D11TextureManager` already uploads DDS bytes into bounded shader-resource-view storage. The native browser does not yet connect those services to the current catalog page.

This slice adds progressive thumbnails only. It does not add tattoo selection, apply, replace, remove, actor controls, or PrismaUI retirement.

## Goals

- Render thumbnails for only the six tattoos on the current native catalog page.
- Show placeholders immediately and progressively replace them with one completed thumbnail at a time.
- Keep filesystem, BSA, DDS, and GPU work outside the render callback.
- Cancel queued work when page, filter, or catalog generation changes.
- Preserve loose-first and BSA-fallback texture resolution.
- Bound native GPU residency by both capacity and idle time.
- Distinguish loading, missing, and broken textures without repeated retries.
- Preserve F1 SKSE Menu Framework behavior and F8 PrismaUI behavior.

## Non-goals

- No clickable tattoo selection or mutation controls.
- No adjacent-page prefetch.
- No background worker pool.
- No native `.rgba` files, Base64 transport, or generated thumbnail cache.
- No shared cache or lifecycle changes for the legacy PrismaUI path.
- No persistent thumbnail state across game sessions.

## Architecture

```text
NativeCatalogBrowserModel::page() (up to six entries)
                    |
                    v
NativeThumbnailController
  generation + visible paths + states + one in-flight request
                    |
                    v
SKSE game-thread task
  TextureResolver -> loose DDS or BSA DDS bytes
                    |
                    v
D3D11TextureManager
  DDS upload + capacity/TTL-bounded SRV cache
                    |
                    v
OfficialMenuFrameworkAdapter
  placeholder/loading/ready/missing/broken card presentation
```

The controller is the concurrency boundary. The adapter synchronizes the current snapshot identity and current page paths, then reads presentation state. It never opens a file, reads a BSA stream, decodes DDS data, or creates a GPU resource.

The controller permits one in-flight request. A page or filter transition increments its generation, clears queued paths, and enqueues only the new page. An already-running task may finish its CPU read, but it must compare the captured generation before GPU upload and discard stale bytes.

## Components

### Native Thumbnail Controller

The controller owns:

- the current catalog snapshot identity;
- a monotonically increasing request generation;
- the six normalized visible texture paths;
- per-path presentation state;
- a FIFO queue for the current page only;
- at most one in-flight request;
- negative-cache entries for missing and broken textures;
- strong texture references for the visible page.

Presentation states are `placeholder`, `loading`, `ready`, `missing`, and `broken`. Synchronizing the same snapshot and paths is idempotent and does not enqueue duplicate work. Returning to a cached page publishes ready entries without resolving or uploading again.

### Native Thumbnail Loader

The loader executes through an SKSE game-thread task. It:

1. validates and normalizes the tattoo texture path;
2. resolves a loose DDS file first;
3. falls back to `RE::BSResourceNiBinaryStream` for BSA content;
4. compares the captured generation before upload;
5. uploads non-stale DDS bytes through `D3D11TextureManager`;
6. publishes a terminal result to the controller.

The loader remains single-request. It does not create a worker pool or dispatch all six textures into one game frame.

### Native D3D11 Cache

The native cache is independent of the PrismaUI thumbnail path. Its initial policy is:

- hard capacity: 12 textures;
- idle TTL: two minutes since last access;
- expiry scan: at most once per second;
- visible textures remain strongly referenced and cannot expire;
- non-visible textures are released by idle expiry or LRU eviction;
- cache and negative-cache state reset when a new catalog snapshot is published;
- all state resets naturally on game restart.

The cache exposes deterministic time injection for tests. Releasing the final shared texture reference releases its `ID3D11ShaderResourceView`.

## Data Flow

Each render pass provides the controller with the immutable snapshot identity and current `TattooPage`. If the page is unchanged, the controller performs no new work. If it changed, the controller creates a new generation, removes queued stale work, retains ready cache hits, and places only uncached current-page paths into its queue.

When no request is active, the controller schedules the next path. Completion publishes one result. The next render can display that result and schedule the following path. This produces progressive cards without synchronous six-texture bursts.

A stale task never publishes into a new page generation. If generation changes before upload, the task discards its DDS bytes. Only the currently visible entries can hold card-level strong references.

## Rendering

The existing two-column by three-row layout remains. Each card gains a fixed thumbnail region above its text metadata.

- `placeholder`: dark or checkerboard region before a request starts;
- `loading`: placeholder plus a loading label;
- `ready`: SRV image fitted into the region without stretching its aspect ratio;
- `missing`: explicit missing-texture placeholder;
- `broken`: explicit broken-texture placeholder.

Cards continue to show name, pack/source, section, area, and texture path. Thumbnail failure never hides catalog metadata. This slice keeps cards read-only.

## Error Handling

- Unsafe paths become `broken` without entering the resolver.
- Loose-file absence falls back to BSA resolution.
- Absence from both loose files and BSA becomes `missing`.
- Read, DDS decode, or GPU upload failure becomes `broken`.
- A temporarily unavailable D3D11 device leaves the entry as `placeholder`; it is retried only after device availability changes and is not negative-cached.
- Missing and broken results are negative-cached for the current catalog epoch and are not retried every frame.
- Task-boundary exceptions are converted to `broken` and logged once per texture per epoch.
- A catalog snapshot replacement clears ready, loading, missing, and broken state.

## Testing

### Controller Tests

- Only the six current-page paths are requested.
- At most one request is in flight.
- Same-page synchronization is idempotent.
- Page and filter changes discard queued stale work.
- Stale generation completion does not upload or publish.
- Cache hits skip resolution and upload.
- Missing and broken entries are not retried in the same epoch.
- Snapshot replacement resets ready and negative-cache state.

### Cache Tests

- A non-visible texture expires after two idle minutes using a fake clock.
- A visible texture remains alive past the TTL.
- Capacity 12 evicts the least-recently-used non-visible texture.
- Eviction and expiry release the final SRV reference when no consumer retains it.

### Loader and Adapter Tests

- Loose resolution precedes BSA fallback.
- Invalid, missing, read, and upload failures map to the correct presentation state.
- D3D11 WARP upload and reuse tests remain green.
- Presentation states map to the correct card treatment.
- Image fitting preserves aspect ratio.

### Verification

- Run focused thumbnail, resolver, texture-manager, and adapter tests.
- Build Debug and Release through `build.ps1`.
- Run both CTest suites and `git diff --check`.
- In game, verify progressive loading, no stale card images after page changes, cache hits when returning one page, loose and BSA thumbnails, and unchanged F8 PrismaUI behavior.

## Acceptance Criteria

- Opening the native browser does not resolve or upload off-page textures.
- The current page displays placeholders immediately and thumbnails progressively.
- No more than one texture request is active.
- Page/filter changes do not display stale textures in reused cards.
- Missing and broken textures do not cause repeated frame-by-frame work.
- Native GPU texture residency is bounded to 12 entries and two idle minutes.
- Returning to the immediately previous page normally uses cache hits.
- Loose and BSA-backed SlaveTats textures render in the native cards.
- Closing and reopening the browser does not leak SRVs or worker state.
- F1 remains SKSE Menu Framework and F8 PrismaUI remains operational.
