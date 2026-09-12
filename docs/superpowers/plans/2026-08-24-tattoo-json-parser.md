# SlaveTats JSON Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Parse each effective SlaveTats JSON source into typed, source-aware tattoo definitions while preserving valid entries and reporting malformed data.

**Architecture:** Keep filesystem discovery in `TattooSourceScanner`; add a parser that consumes one discovered source and returns a report containing typed definitions plus issues. The `DataLoaded` tracer aggregates counts only, leaving runtime matching and UI changes for the next repository slice.

**Tech Stack:** C++23, nlohmann/json, CMake/CTest, CommonLibSSE-NG logging.

**Spec:** `SKSE_MENU_MIGRATION.md` Phase 2.

## Global Constraints

- Preserve `sourceId`, `sourceFile`, and `packName` from the effective source descriptor.
- Require string fields `name`, `section`, `texture`, and `area` for each accepted entry.
- Preserve optional `glow`, `in_bsa`, and `credit` when their JSON types are valid.
- Skip malformed entries with an indexed issue instead of discarding valid siblings.
- Do not change Prisma payloads, query behavior, or texture loading.
- Do not commit without explicit user approval.

---

### Task 1: Typed source parser

**Files:**
- Create: `src/repository/TattooSourceParser.h`
- Create: `src/repository/TattooSourceParser.cpp`
- Create: `tests/repository/TattooSourceParserTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `const TattooSourceFile&`.
- Produces: `TattooSourceParseReport parseTattooSource(const TattooSourceFile&)` containing `definitions` and `issues`.

- [x] **Step 1: Write failing tests**

Use real temporary JSON files to assert source metadata propagation, required fields, optional metadata, root-level parse errors, and one malformed sibling being skipped while a valid sibling remains.

- [x] **Step 2: Verify RED**

Build `TattooSourceParserTests`; expected failure: missing `repository/TattooSourceParser.h`.

- [x] **Step 3: Implement minimal parser**

Define `TattooDefinition`, `TattooParseIssue`, and `TattooSourceParseReport`. Parse without throwing on invalid JSON; validate required/optional fields and retain the source entry index.

- [x] **Step 4: Verify GREEN**

Run the focused test target; expected PASS.

### Task 2: Parse tracer integration

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: effective sources returned by `scanTattooSources`.
- Produces: summary log with source, tattoo-definition, and parse-issue counts.

- [x] **Step 1: Add parser files to plugin/test targets**

Link nlohmann/json to the parser test target and include parser source in the plugin.

- [x] **Step 2: Aggregate parse reports after a successful scan**

Parse every effective source, log each issue with source ID and optional entry index, and emit one summary. Parser failures must not block `Bridge::onDataLoaded()`.

- [x] **Step 3: Verify complete build**

Build Debug and Release, run full CTest, run `git diff --check`, and inspect the diff for absolute MO2 paths.

## Self-Review

- Spec coverage: typed parsing and provenance are covered; runtime matching, indexes, filtering, and native UI remain later slices.
- Placeholder scan: no placeholders are present.
- Type consistency: parser inputs and source descriptor types use the scanner contract unchanged.
