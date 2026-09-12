# Tattoo Repository Design

## Context

The effective-source scanner and typed JSON parser now discover 46 MO2-resolved SlaveTats JSON sources and parse 792 definitions with zero issues in the user's current game. The parsed definitions are currently used only for startup diagnostics. Phase 2 of `SKSE_MENU_MIGRATION.md` requires an in-memory, source-aware query model before native UI work begins.

## Goal

Add a UI-independent `TattooRepository` that owns parsed `TattooDefinition` metadata and returns deterministic, paginated results for combined text, source, section, and area filters without performing runtime queries or texture I/O.

## Non-goals

- Do not merge JSON definitions with actor-specific `core::TattooEntry` runtime state yet.
- Do not change Prisma payloads, `Bridge` dispatch, SlaveTatsNG calls, or texture loading.
- Do not add ImGui or SKSE Menu Framework integration.
- Do not add an inverted-index engine before profiling demonstrates a need.

## Data ownership and API

`TattooRepository` lives in `src/repository` and owns the definitions supplied at construction. JSON definitions remain separate from `core::TattooEntry`: the former describe the available library and its source provenance, while the latter describe runtime/JContainers state for an actor.

```cpp
struct TattooFilter {
    std::string search;
    std::string sourceId;
    std::string section;
    std::string area;
    std::size_t pageIndex{0};
    std::size_t pageSize{24};
};

struct TattooPage {
    std::vector<TattooDefinition> entries;
    std::size_t totalEntries{};
    std::size_t matchedEntries{};
    std::size_t pageIndex{};
    std::size_t pageSize{24};
    std::size_t pageCount{};
};

struct TattooSourceOption {
    std::string sourceId;
    std::string packName;
};

struct TattooFacets {
    std::vector<TattooSourceOption> sources;
    std::vector<std::string> sections;
    std::vector<std::string> areas;
};

class TattooRepository {
public:
    explicit TattooRepository(std::vector<TattooDefinition> definitions);

    [[nodiscard]] TattooPage query(const TattooFilter& filter = {}) const;
    [[nodiscard]] const TattooFacets& facets() const noexcept;
};
```

Page results copy only the visible definitions, avoiding dangling references while bounding copy cost to the requested page size.

## Normalization and filtering

The constructor precomputes an ASCII-case-folded search record for each definition. Non-ASCII bytes remain unchanged. Search matches substrings across `name`, `packName`, `sourceFile`, `section`, `area`, and `texturePath`. Source, section, and area filters use folded exact equality. Empty filter values are wildcards.

Filtering scans only precomputed metadata. It never opens a DDS, queries SlaveTatsNG/JContainers, or touches the filesystem. An O(N) metadata scan is appropriate for the current 792 entries and the several-thousand-entry target; inverted indexes remain a measured future optimization.

## Ordering and pagination

Definitions are sorted once at construction by folded `packName`, `section`, `area`, `name`, and `texturePath`, followed by `sourceId` and `sourceIndex` as deterministic tie-breakers.

`pageIndex` is zero-based internally. A `pageSize` of zero uses the default of 24. If a non-empty result requests a page beyond the last page, the repository clamps it to the last page. An empty result reports `pageIndex = 0` and `pageCount = 0`.

The repository reports both the complete library size and the filtered match count so a future UI can display “Showing X of Y tattoos.”

## Facets

The constructor derives stable, duplicate-free source, section, and area options from the owned definitions. Sources are identified by `sourceId` and display `packName`; identical display names from different source files remain distinct. Facet ordering follows the same folded display ordering used by query results.

## Error handling

The repository accepts an empty definition vector and returns an empty page/facets. Parsing and filesystem errors remain the parser/scanner's responsibility. Repository queries are deterministic and do not return runtime errors because they perform only in-memory operations.

## Testing

Unit tests use real `TattooDefinition` fixtures and cover:

- deterministic ordering independent of input order,
- case-insensitive text search across metadata,
- combined source, section, and area filters,
- default pagination and matched/total counts,
- out-of-range page clamping,
- zero page size fallback,
- empty repositories and empty matches,
- duplicate-free, source-aware facets,
- a synthetic library larger than 2,000 definitions without texture or runtime dependencies.

The repository target is added to CMake/CTest. The complete Debug and Release builds and full test suites must pass before the slice is considered complete.

## Integration boundary

This slice delivers the pure repository and tests. Startup ownership and native/Prisma consumers will be added only when a composition root can own the repository without introducing a singleton or coupling it to `Bridge` prematurely.
