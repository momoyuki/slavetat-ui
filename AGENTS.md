# Agent Guide

This file defines how coding agents should recover project context and work safely in this repository.

## Read this before planning or changing code

For every non-trivial task:

1. Read `ROADMAP.md` to understand the active milestone and project direction.
2. Read `CONTEXT.md` for project terminology and workflow concepts.
3. Read the relevant design in `docs/superpowers/specs/`.
4. Read the relevant approved implementation plan in `docs/superpowers/plans/active/` when one exists.
5. Inspect the current implementation and tests before proposing changes.
6. Check open issues and pull requests when they may overlap the requested work.

Historical plans under `docs/superpowers/plans/archive/` are implementation records only. They are useful for design archaeology, debugging, and regression context, but they are **not** evidence that unchecked tasks are still pending.

Do not infer the project's direction from the latest PR or an archived plan alone.

## Source-of-truth hierarchy

Use these documents for different questions:

- `ROADMAP.md` — **what the project is trying to achieve and milestone order**
- `CONTEXT.md` — **shared language and mental model**
- `docs/superpowers/specs/` — **behavior and architecture contracts**
- `docs/superpowers/plans/active/` — **implementation sequence for approved current work**
- `docs/superpowers/plans/archive/` — **historical implementation records only**
- tests and current source — **what is actually implemented now**
- GitHub issues / PRs — **execution status and discussion**

When two sources conflict, do not silently choose one. Identify the conflict and resolve it against the user's current instruction and the authoritative document for that kind of decision.

## Architecture boundaries

Preserve the existing layering:

```text
Native UI / adapter
        |
        v
Workflow model
        |
        v
Workflow runtime / scheduler
        |
        v
SlaveTatsService
        |
        v
ITattooRuntime
        |
        v
SlaveTatsRuntime
        |
        +--> SlaveTatsNG API
        +--> JContainers
```

Rules:

- Native UI/render code expresses intent and presentation only.
- Workflow models own deterministic, copyable UI state.
- Runtime coordinators own scheduled execution and in-flight protection.
- `SlaveTatsService` owns transport-independent validation and typed contracts.
- `SlaveTatsRuntime` owns actor lookup, SlaveTatsNG calls, JContainers access, and synchronization behavior.
- Do not add direct JContainers or SlaveTatsNG calls to the UI adapter or workflow model.

## Runtime safety invariants

These are not optional conveniences:

- SlaveTatsNG remains the authoritative tattoo runtime.
- External overlay slots remain read-only unless a future approved design explicitly changes that rule.
- Session-local tattoo handles must be revalidated against the requested actor before mutation.
- Actor identity must not be inferred from stale UI state.
- Never silently fall back to the Player when an explicit actor target fails.
- Preserve synchronization-only retry when appearance/storage mutation has already succeeded but visual synchronization failed.
- Do not repeat a completed mutation during a synchronization-only retry.
- Do not bypass the scheduler/runtime boundary for convenience.

## Scope discipline

Before implementation, identify the active roadmap milestone.

Do not silently expand a task into adjacent roadmap items. In particular, the following should normally remain separate unless the user explicitly promotes them into scope:

- Lock / Unlock
- Domain selector
- NPC targeting
- live actor preview
- favorites / recently used
- presets / loadouts
- arbitrary texture-path editing

If a small prerequisite is discovered, document why it is necessary and keep the change minimal.

Avoid broad refactors unless they are required to implement or verify the active milestone.

## Current priority

Read `ROADMAP.md` for the current priority, then follow its linked design and active implementation plan. Do not copy milestone-specific requirements into this guide or treat an archived plan as a substitute.

## Testing expectations

For implementation work, add tests at the narrowest stable seam first and cover the layers touched by the change.

Expected areas include:

- Core service validation
- runtime snapshot/readback
- JContainers write/readback behavior
- stale-handle rejection
- workflow state transitions
- scheduler/in-flight behavior
- adapter presentation helpers
- repository/parser compatibility when metadata changes

For appearance work, explicitly test:

- default/missing advanced fields from older tattoo packs;
- glow color channel order;
- emissive multiplier values including zero and values greater than one;
- transitions from glowing to non-glowing state;
- replacement/removal after glow or bump state was present;
- synchronization failure followed by synchronization-only retry;
- no writes for stale/foreign handles.

Before proposing merge for implementation work, run the repository's relevant Debug and Release builds/test suites where available, `git diff --check`, and review the scoped diff for accidental unrelated changes.

## Documentation behavior

When a design decision changes project direction, update `ROADMAP.md`.

When behavior or architecture changes inside a milestone, update the relevant spec.

When implementation sequencing changes, update the relevant plan under `docs/superpowers/plans/active/`.

When an implementation plan is completed or superseded, move it to `docs/superpowers/plans/archive/` rather than deleting it.

Do not duplicate large design documents into `ROADMAP.md`; keep the roadmap concise enough to recover project intent quickly.

## Pull request expectations

Prefer focused PRs with a clear relationship to one roadmap milestone.

A useful PR description should state:

- roadmap milestone;
- problem being solved;
- scope and non-goals;
- architecture boundaries affected;
- user-visible behavior;
- tests performed;
- known risks or follow-ups.

Do not merge, deploy, release, or broaden scope without explicit user approval when approval has not already been given.
