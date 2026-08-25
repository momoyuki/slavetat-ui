# Native Catalog Browser Design

## Context

The native menu foundation is registered through SKSE Menu Framework while PrismaUI remains the F8 frontend. `TattooCatalogStore` already publishes an immutable `TattooCatalogSnapshot` after the existing `kDataLoaded` JSON scan. Its `TattooRepository` supports deterministic filtered pages and exposes source, section, and area facets without texture I/O.

The next slice adds a read-only native catalog browser. It must avoid the PrismaUI regression where selecting a tattoo returns the user to page one. This slice creates no tattoo mutation controls, so it establishes state ownership before future selection and apply flows are added.

## Goals

- Render a read-only catalog page in the native Tattoo Browser.
- Filter by text search, source, section, and area.
- Show six entries per page in a two-column, three-row layout.
- Provide `Prev`, `Page [input] / total`, and `Next` controls.
- Preserve search, filters, and page state while the game session remains active, including after closing and reopening Browse.
- Reset the page to one only when a filter changes or a successful catalog refresh publishes a new snapshot.
- Keep F1 as SKSE Menu Framework and F8 as PrismaUI.
- Perform no DDS decode, D3D11 upload, SlaveTatsNG query, or JContainers access from the render callback.

## Non-goals

- No thumbnail rendering in this slice.
- No actor, slot, apply, replace, remove, color, alpha, or synchronization controls.
- No persisted configuration across game sessions.
- No pagination-size setting; the fixed initial page size is six.
- No changes to PrismaUI transport, view files, or hotkeys.

## Architecture

```text
TattooCatalogStore::snapshot()
        |
        v
NativeCatalogBrowserModel
  filter + page index + cached TattooPage
        |
        v
OfficialMenuFrameworkAdapter
  ImGui controls and read-only cards
```

`NativeCatalogBrowserModel` owns native presentation state and depends only on the repository snapshot types. It observes the current immutable snapshot during rendering. It queries the repository only after a filter or page transition, then retains the returned `TattooPage` for rendering.

`OfficialMenuFrameworkAdapter` owns ImGui calls. It forwards user intent to the model and renders the cached page. It does not inspect repository internals or own paging rules.

The composition root passes a snapshot provider referencing the existing process-lifetime `TattooCatalogStore` to the native browser model. No repository snapshot is copied or rebuilt for rendering.

## State and Pagination

The model stores:

- search text,
- selected source ID, section, and area,
- zero-based page index,
- a fixed page size of six,
- the snapshot identity used for the cached page,
- the current cached `TattooPage`.

The UI displays a one-based page number. Committing the page input by Enter or focus loss converts it to zero-based form and clamps it to `[1, pageCount]`. An empty result displays page `0 / 0`; Prev, Next, and page input changes do nothing.

Changing any filter resets the page index to zero and refreshes the cached page. Prev and Next stay within the available range. Opening or closing Browse does not alter model state. A new successful snapshot clears filters, resets page one, and refreshes the page. A failed refresh retains the prior snapshot and therefore preserves browser state.

## Rendering

The browser renders:

1. search field;
2. source, section, and area selectors from repository facets;
3. a two-column, three-row read-only card grid;
4. each card's name, pack/source, section, area, and texture path;
5. `Prev`, `Page [input] / total`, and `Next` controls;
6. an explicit empty-catalog or no-match message.

Cards are intentionally text-only. The following thumbnail slice may replace or extend them without changing model pagination state.

## Error Handling

- No snapshot: render an empty-catalog state; do not attempt a repository query.
- Empty filtered result: render a no-match message with disabled navigation.
- Invalid page input: clamp safely; never use it as a raw vector index.
- Snapshot replacement: invalidate cached page before rendering from the new snapshot.
- Render callback exception: retain the native foundation's existing ABI boundary protection.

## Testing

Automated tests cover:

- six-entry pages and deterministic order;
- filter combinations and their page-one reset;
- Prev/Next boundaries;
- one-based page input conversion and clamping;
- state survival across close/open presentation events;
- successful snapshot replacement reset;
- failed refresh retention;
- no snapshot and no-match presentation states;
- Debug and Release CTest suites.

In-game acceptance:

1. F1 opens only SKSE Menu Framework.
2. `Open Tattoo Browser` opens the native panel.
3. Search and each facet reduce the list without texture loading.
4. The panel shows six large cards per page.
5. Page input jumps to a requested valid page; Prev and Next navigate correctly.
6. Closing and reopening Browse retains search, filters, and page.
7. Changing a filter returns to page one.
8. F8 PrismaUI remains functional.

## Follow-up

The next slice adds current-page-only thumbnail requests through `TextureResolver` and `D3D11TextureManager`. Actor and tattoo mutation controls follow only after thumbnail and catalog UX are validated.
