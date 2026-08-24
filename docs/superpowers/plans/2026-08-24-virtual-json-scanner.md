# Virtual SlaveTats JSON Scanner Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Discover the effective loose SlaveTats JSON files visible to the Skyrim process through MO2 and log their stable virtual identities after `DataLoaded`.

**Architecture:** Add a UI-independent filesystem scanner that accepts the effective SlaveTats directory as an injected path. It returns sorted source descriptors without parsing tattoo entries or depending on Skyrim, then `main.cpp` locates `<Skyrim>/Data/textures/actors/character/SlaveTats` and logs the tracer result.

**Tech Stack:** C++23, `std::filesystem`, CMake/CTest, CommonLibSSE-NG logging.

**Spec:** `SKSE_MENU_MIGRATION.md` Phase 2.

## Global Constraints

- Treat MO2 overwrite resolution as authoritative; do not inspect physical MO2 mod directories at runtime.
- Scan loose effective JSON only; BSA texture resolution remains unchanged.
- Normalize source identity case-insensitively to `textures/actors/character/slavetats/<lowercase filename>`.
- Do not parse JSON tattoo entries or alter Prisma UI behavior in this tracer slice.
- Do not commit without explicit user approval.

---

### Task 1: Effective JSON scanner

**Files:**
- Create: `src/repository/TattooSourceScanner.h`
- Create: `src/repository/TattooSourceScanner.cpp`
- Create: `tests/repository/TattooSourceScannerTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `std::filesystem::path` pointing at the effective SlaveTats directory.
- Produces: `std::expected<std::vector<TattooSourceFile>, std::error_code> scanTattooSources(const std::filesystem::path&)`.

- [ ] **Step 1: Write the failing scanner tests**

Create real temporary files `PackB.JSON`, `PackA.json`, `notes.txt`, and `nested/Ignored.json`. Assert that scanning returns exactly two direct JSON sources, sorted by normalized `sourceId`, with original `sourceFile` and filename-stem `packName`. Assert that a missing directory returns an error.

- [ ] **Step 2: Run the scanner test and verify RED**

Run `cmake --preset build-debug -DBUILD_TESTING=ON` and build `TattooSourceScannerTests`. Expected: compilation fails because `repository/TattooSourceScanner.h` does not exist.

- [ ] **Step 3: Implement the minimum scanner**

Define:

```cpp
struct TattooSourceFile {
    std::string sourceId;
    std::string sourceFile;
    std::string packName;
    std::filesystem::path path;
};

using TattooSourceScanResult =
    std::expected<std::vector<TattooSourceFile>, std::error_code>;

[[nodiscard]] TattooSourceScanResult scanTattooSources(
    const std::filesystem::path& directory);
```

Iterate only the supplied directory, accept `.json` case-insensitively, normalize the virtual ID, and sort by `sourceId`. Return filesystem errors rather than treating them as an empty library.

- [ ] **Step 4: Run the focused test and verify GREEN**

Build and run `TattooSourceScannerTests`; expected: PASS.

### Task 2: In-game tracer logging

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Skyrim executable directory from `GetModuleFileNameW`.
- Produces: one summary log and one debug log per effective JSON source after `DataLoaded`.

- [ ] **Step 1: Register scanner production files in CMake**

Add scanner header/source to the plugin target and scanner source/test to a dedicated CTest target.

- [ ] **Step 2: Connect the tracer after `DataLoaded`**

Build the directory path from the executable parent plus `Data/textures/actors/character/SlaveTats`. Call `scanTattooSources`; on error log the directory and error message, otherwise log source count and each `sourceId`/`packName`. Do not block Prisma initialization if scanning fails.

- [ ] **Step 3: Verify the complete build**

Configure Debug, build the plugin and all tests, run CTest, run `git diff --check`, and confirm the working diff contains no MO2 absolute paths.

## Self-Review

- Spec coverage: this tracer validates effective loose JSON discovery only; parsing, runtime matching, indexes, native UI, and BSA texture work remain separate slices.
- Placeholder scan: no implementation placeholders are used.
- Type consistency: the scanner result and descriptor names are identical in tests, production, and integration.
