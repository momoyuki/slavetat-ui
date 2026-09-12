# Tattoo Repository Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a UI-independent, source-aware in-memory repository that filters, deterministically orders, and paginates parsed SlaveTats JSON definitions without runtime or texture I/O.

**Architecture:** `TattooRepository` owns `TattooDefinition` values from `TattooSourceParser` plus precomputed ASCII-folded metadata. Queries scan only this metadata, copy the requested page, and expose stable facets for future UI filters. Runtime/JContainers entries remain separate.

**Tech Stack:** C++23, CMake, CTest, existing custom C++ test harness.

**Spec:** `docs/superpowers/specs/2026-08-24-tattoo-repository-design.md`

## Global Constraints

- Preserve every `TattooDefinition` field unchanged in returned entries.
- Do not modify Prisma payloads, `Bridge`, SlaveTatsNG/JContainers calls, or texture loading.
- Do not read files or decode textures during repository queries.
- Use ASCII-only case folding; preserve non-ASCII bytes.
- Keep source identity separate from `packName` and `section`.
- Default to 24 entries per page and clamp invalid page indexes.
- Do not commit without explicit user approval. At each commit gate, show the diff and proposed Conventional Commit message first.

## File Map

- Create `src/repository/TattooRepository.h`: public filters, page/facet results, and repository contract.
- Create `src/repository/TattooRepository.cpp`: normalization, ordering, filtering, pagination, and facet construction.
- Create `tests/repository/TattooRepositoryTests.cpp`: real metadata fixtures covering repository behavior and scale.
- Modify `CMakeLists.txt`: add repository files to the plugin and register `TattooRepositoryTests`.

### Task 1: Deterministic ownership and pagination

**Files:**
- Create: `src/repository/TattooRepository.h`
- Create: `src/repository/TattooRepository.cpp`
- Create: `tests/repository/TattooRepositoryTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `std::vector<TattooDefinition>` from `TattooSourceParser.h`.
- Produces: `TattooFilter`, `TattooPage`, `TattooSourceOption`, `TattooFacets`, and `TattooRepository`.

- [ ] **Step 1: Write ordering and pagination tests before production files**

Create `tests/repository/TattooRepositoryTests.cpp` with the existing `expect`/`run` harness and these fixtures/tests:

```cpp
TattooDefinition definition(
    std::string sourceId,
    std::string pack,
    std::string section,
    std::string name,
    std::string texture,
    std::string area,
    std::size_t sourceIndex = 0) {
    return TattooDefinition{
        .sourceId = std::move(sourceId),
        .sourceFile = pack + ".json",
        .packName = std::move(pack),
        .sourceIndex = sourceIndex,
        .name = std::move(name),
        .section = std::move(section),
        .texturePath = std::move(texture),
        .area = std::move(area),
    };
}

void ordersDefinitionsDeterministically() {
    TattooRepository repository({
        definition("z.json", "Pack B", "Marks", "Zulu", "z.dds", "Body"),
        definition("a.json", "pack a", "Marks", "Beta", "b.dds", "Body", 1),
        definition("a.json", "pack a", "Marks", "Alpha", "a.dds", "Body", 0),
    });

    const auto page = repository.query();

    expect(page.entries.size() == 3, "expected all definitions on the default page");
    expect(page.entries[0].name == "Alpha", "expected folded pack/name ordering");
    expect(page.entries[1].name == "Beta", "expected source index tie-break order");
    expect(page.entries[2].name == "Zulu", "expected later pack last");
    expect(page.totalEntries == 3 && page.matchedEntries == 3, "expected total and match counts");
    expect(page.pageIndex == 0 && page.pageSize == 24 && page.pageCount == 1, "expected default paging");
}

void paginatesAndClampsOutOfRangePage() {
    std::vector<TattooDefinition> definitions;
    for (std::size_t index = 0; index < 5; ++index) {
        definitions.push_back(definition(
            "pack.json", "Pack", "Marks", "Mark " + std::to_string(index),
            std::to_string(index) + ".dds", "Body", index));
    }
    TattooRepository repository(std::move(definitions));

    const auto page = repository.query(TattooFilter{.pageIndex = 9, .pageSize = 2});

    expect(page.pageIndex == 2 && page.pageCount == 3, "expected page index clamped to last page");
    expect(page.entries.size() == 1, "expected one entry on final page");
    expect(page.entries.front().name == "Mark 4", "expected deterministic final entry");
}

void zeroPageSizeUsesDefault() {
    TattooRepository repository({definition("pack.json", "Pack", "Marks", "Mark", "mark.dds", "Body")});
    const auto page = repository.query(TattooFilter{.pageSize = 0});
    expect(page.pageSize == 24, "expected zero page size to use default");
}
```

Register all three functions in `main()`.

- [ ] **Step 2: Add only the test target and verify RED**

Add to the existing `if(BUILD_TESTING)` block:

```cmake
add_executable(TattooRepositoryTests
    tests/repository/TattooRepositoryTests.cpp
)
target_include_directories(TattooRepositoryTests PRIVATE src)
add_test(NAME TattooRepositoryTests COMMAND TattooRepositoryTests)
```

Run:

```powershell
cmake --preset build-debug -DBUILD_TESTING=ON
cmake --build build/debug --target TattooRepositoryTests
```

Expected: compilation fails because `repository/TattooRepository.h` does not exist.

- [ ] **Step 3: Add the public repository contract**

Create `src/repository/TattooRepository.h`:

```cpp
#pragma once

#include "repository/TattooSourceParser.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stui::repository {

inline constexpr std::size_t kDefaultTattooPageSize = 24;

struct TattooFilter {
    std::string search;
    std::string sourceId;
    std::string section;
    std::string area;
    std::size_t pageIndex{};
    std::size_t pageSize{kDefaultTattooPageSize};
};

struct TattooPage {
    std::vector<TattooDefinition> entries;
    std::size_t totalEntries{};
    std::size_t matchedEntries{};
    std::size_t pageIndex{};
    std::size_t pageSize{kDefaultTattooPageSize};
    std::size_t pageCount{};
};

struct TattooSourceOption {
    std::string sourceId;
    std::string packName;
    bool operator==(const TattooSourceOption&) const = default;
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

private:
    struct IndexedDefinition {
        TattooDefinition definition;
        std::string foldedSearch;
        std::string foldedSourceId;
        std::string foldedSection;
        std::string foldedArea;
        std::string foldedPackName;
        std::string foldedName;
        std::string foldedTexturePath;
    };

    std::vector<IndexedDefinition> m_entries;
    TattooFacets m_facets;
};

}  // namespace stui::repository
```

- [ ] **Step 4: Implement construction, ordering, and pagination minimally**

Create `src/repository/TattooRepository.cpp`. Implement `foldASCII` by changing only bytes `A` through `Z`; build `foldedSearch` by joining folded searchable fields with newline delimiters. Sort `m_entries` with `std::tie` over folded pack, section, area, name, texture, then original `sourceId` and `sourceIndex`.

For `query`, initially treat every entry as a match. Compute:

```cpp
const std::size_t pageSize = filter.pageSize == 0 ? kDefaultTattooPageSize : filter.pageSize;
const std::size_t matchedEntries = m_entries.size();
const std::size_t pageCount = matchedEntries / pageSize + (matchedEntries % pageSize != 0 ? 1 : 0);
const std::size_t pageIndex = pageCount == 0 ? 0 : std::min(filter.pageIndex, pageCount - 1);
const std::size_t begin = pageIndex * pageSize;
const std::size_t end = std::min(begin + pageSize, matchedEntries);
```

Copy definitions in `[begin, end)` into `TattooPage::entries`. Return `m_facets` unchanged from `facets()`; Task 3 populates it.

- [ ] **Step 5: Wire production sources and verify GREEN**

Add `src/repository/TattooRepository.h/.cpp` to the plugin header/source lists. Add `src/repository/TattooRepository.cpp` to `TattooRepositoryTests`.

Run:

```powershell
.\build.ps1 -Config debug
.\build\debug\TattooRepositoryTests.exe
```

Expected: all ordering and pagination tests print `PASS` and return 0.

### Task 2: Combined metadata filtering

**Files:**
- Modify: `tests/repository/TattooRepositoryTests.cpp`
- Modify: `src/repository/TattooRepository.cpp`

**Interfaces:**
- Consumes: `TattooFilter` fields defined in Task 1.
- Produces: case-insensitive substring search and exact folded facet filters.

- [ ] **Step 1: Add failing filter tests**

Add and register:

```cpp
void searchesAcrossMetadataCaseInsensitively() {
    TattooRepository repository({
        definition("rose.json", "Rose Pack", "Flowers", "Red Rose", "rose/red.dds", "Body"),
        definition("rune.json", "Rune Pack", "Magic", "Blue Rune", "rune/blue.dds", "Face"),
    });

    const auto page = repository.query(TattooFilter{.search = "ROSE/RED"});

    expect(page.matchedEntries == 1, "expected folded texture-path search");
    expect(page.entries.front().name == "Red Rose", "expected rose result");
    expect(page.totalEntries == 2, "expected unfiltered total preserved");
}

void combinesSourceSectionAndAreaFilters() {
    TattooRepository repository({
        definition("one.json", "One", "Marks", "Body One", "one.dds", "Body"),
        definition("one.json", "One", "Marks", "Face One", "face.dds", "Face"),
        definition("two.json", "Two", "Marks", "Body Two", "two.dds", "Body"),
    });

    const auto page = repository.query(TattooFilter{
        .sourceId = "ONE.JSON",
        .section = "marks",
        .area = "BODY",
    });

    expect(page.matchedEntries == 1, "expected all exact filters combined");
    expect(page.entries.front().name == "Body One", "expected only matching source/section/area");
}

void emptyMatchResetsPaging() {
    TattooRepository repository({definition("one.json", "One", "Marks", "Mark", "one.dds", "Body")});
    const auto page = repository.query(TattooFilter{.search = "missing", .pageIndex = 8, .pageSize = 12});
    expect(page.entries.empty(), "expected empty page");
    expect(page.matchedEntries == 0 && page.pageCount == 0, "expected zero match/page counts");
    expect(page.pageIndex == 0 && page.pageSize == 12, "expected empty result paging normalized");
}
```

- [ ] **Step 2: Verify RED against the unfiltered implementation**

Run `cmake --build build/debug --target TattooRepositoryTests` and `build/debug/TattooRepositoryTests.exe`.

Expected: the new search/filter tests fail because every definition still matches.

- [ ] **Step 3: Implement metadata-only matching**

Fold all incoming filters once per query. An entry matches when each non-empty exact filter equals its corresponding folded field and an empty or folded search string is contained in `foldedSearch`.

Collect pointers to matching `IndexedDefinition` values in sorted repository order, then apply the pagination calculation from Task 1 to that match vector. Set `totalEntries` to `m_entries.size()` and `matchedEntries` to the match vector size.

- [ ] **Step 4: Verify GREEN**

Run `cmake --build build/debug --target TattooRepositoryTests` and `build/debug/TattooRepositoryTests.exe`.

Expected: all ordering, pagination, and filter tests pass.

### Task 3: Stable facets and large-library behavior

**Files:**
- Modify: `tests/repository/TattooRepositoryTests.cpp`
- Modify: `src/repository/TattooRepository.cpp`

**Interfaces:**
- Consumes: definitions owned by `TattooRepository`.
- Produces: stable `TattooFacets` and metadata-only behavior for 2,000+ definitions.

- [ ] **Step 1: Add failing facet and scale tests**

Add and register:

```cpp
void buildsStableSourceAwareFacets() {
    TattooRepository repository({
        definition("b.json", "Same Pack", "Marks", "B", "b.dds", "BODY"),
        definition("a.json", "Same Pack", "marks", "A", "a.dds", "Body"),
        definition("c.json", "Other", "Runes", "C", "c.dds", "Face"),
    });

    const auto& facets = repository.facets();

    expect(facets.sources.size() == 3, "expected identical pack labels to retain distinct sources");
    expect(facets.sources[0] == TattooSourceOption{.sourceId = "c.json", .packName = "Other"}, "expected folded source display ordering");
    expect(facets.sections == std::vector<std::string>{"Marks", "Runes"}, "expected folded duplicate-free sections");
    expect(facets.areas == std::vector<std::string>{"BODY", "Face"}, "expected folded duplicate-free areas");
}

void queriesLargeLibraryWithoutExternalDependencies() {
    std::vector<TattooDefinition> definitions;
    definitions.reserve(2048);
    for (std::size_t index = 0; index < 2048; ++index) {
        definitions.push_back(definition(
            "pack-" + std::to_string(index % 8) + ".json",
            "Pack " + std::to_string(index % 8),
            index % 2 == 0 ? "Even" : "Odd",
            "Tattoo " + std::to_string(index),
            "texture-" + std::to_string(index) + ".dds",
            index % 2 == 0 ? "Body" : "Face",
            index));
    }
    TattooRepository repository(std::move(definitions));

    const auto page = repository.query(TattooFilter{.section = "even", .pageIndex = 1, .pageSize = 48});

    expect(page.totalEntries == 2048 && page.matchedEntries == 1024, "expected complete metadata counts");
    expect(page.entries.size() == 48 && page.pageCount == 22, "expected bounded page from large library");
}
```

- [ ] **Step 2: Verify RED because facets are still empty**

Run `cmake --build build/debug --target TattooRepositoryTests` and `build/debug/TattooRepositoryTests.exe`.

Expected: the facet test fails with an empty `sources` vector; the scale query remains a regression guard.

- [ ] **Step 3: Build deterministic facets at construction**

Build source options from unique folded `sourceId` values, retaining distinct source IDs even when pack labels match. Sort sources by folded `packName`, then folded `sourceId`.

For sections and areas, sort candidate display values by their folded value, then original value, and retain the first display spelling for each folded value. This makes `Marks`/`marks` and `BODY`/`Body` one filter option while keeping deterministic display text.

- [ ] **Step 4: Verify the repository target**

Run:

```powershell
cmake --build build/debug --target TattooRepositoryTests
.\build\debug\TattooRepositoryTests.exe
```

Expected: every repository test passes.

### Task 4: Full verification and review

**Files:**
- Review all files changed by Tasks 1-3.

**Interfaces:**
- Validates the repository contract against the design without changing runtime integration.

- [ ] **Step 1: Run complete Debug verification**

```powershell
.\build.ps1 -Config debug
ctest --test-dir build/debug --output-on-failure
```

Expected: Debug plugin build succeeds and all CTest targets pass.

- [ ] **Step 2: Run complete Release verification**

```powershell
.\build.ps1 -Config release
ctest --test-dir build/release --output-on-failure
```

Expected: Release plugin build succeeds and all CTest targets pass.

- [ ] **Step 3: Inspect quality and scope**

Run:

```powershell
git diff --check
rg -n "D:[\\/]|Modding\\|Desktop\\Desktop" src/repository tests/repository CMakeLists.txt
rg -n "\[DEBUG-" src tests
```

Expected: no whitespace errors, machine-specific paths, or temporary debug instrumentation.

- [ ] **Step 4: Run two-axis code review**

Use the `code-review` skill against the fixed point at the start of this repository slice. Standards review uses repository instructions plus the smell baseline; spec review uses `docs/superpowers/specs/2026-08-24-tattoo-repository-design.md`. Address findings and repeat the relevant tests.

- [ ] **Step 5: Present commit gate**

Show the final diff summary, verification evidence, and proposed message:

```text
feat: add source-aware tattoo repository
```

Do not run `git commit` until the user explicitly approves it.

## Self-Review

- Spec coverage: ownership, normalization, combined filtering, deterministic ordering, pagination, facets, empty behavior, and 2,000+ metadata scale are mapped to explicit tests and implementation steps.
- Scope: runtime merging, UI integration, Prisma changes, and texture work remain excluded.
- Placeholder scan: no deferred implementation language or unspecified error handling remains.
- Type consistency: all tasks use `TattooDefinition`, `TattooFilter`, `TattooPage`, `TattooSourceOption`, `TattooFacets`, and `TattooRepository` with the signatures defined in Task 1.
