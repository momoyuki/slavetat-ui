# Native Edit Appearance Design

## Context

The native slot-first workflow can inspect the Player's Current Slots, replace or remove a SlaveTats-managed tattoo, and choose color and alpha while previewing a replacement. An occupied SlaveTats slot already carries a copied session-local runtime handle, current color, visible alpha, and texture path in its Slot Snapshot.

The next migration slice adds an `Edit Appearance` action for an occupied SlaveTats-managed Current Slot. It changes only color and alpha; it does not replace the tattoo. The mutation must be shared by the native workflow and the existing Prisma `updateTattoo` action so environment-specific JContainers behavior has one implementation.

## Goals

- Add `Edit Appearance` to Slot Actions for SlaveTats-managed Current Slots.
- Initialize the editor from the selected Slot Snapshot.
- Update the thumbnail locally while editing without mutating the actor.
- Enable Save only when color or alpha differs from the original appearance.
- Revalidate the copied runtime handle against the actor's currently applied tattoos before mutation.
- Update color and SlaveTats inverted alpha, mark the actor updated, and synchronize once.
- Refresh only the Selected Area after a successful Save.
- Preserve the current Prisma `updateTattoo` JSON contract while routing it through the shared service.
- Support synchronization-only retry after the appearance was changed but synchronization failed.

## Non-Goals

- No tattoo replacement through the appearance editor.
- No lock, glow, gloss, slot, section, name, domain, or texture editing.
- No editing of empty or external Current Slots.
- No NPC selector in the native workflow.
- No live Slot Snapshot polling.
- No native JContainers or SlaveTatsNG calls from the model or ImGui adapter.
- No Prisma payload or response-shape redesign.
- No automatic rollback after synchronization failure.

## Approved User Flow

1. Select an occupied SlaveTats-managed Current Slot to open Slot Actions.
2. Choose `Edit Appearance` to open a separate editor.
3. Color, Alpha, and the tinted thumbnail initialize from the Slot Snapshot.
4. Changing Color or Alpha updates editor state and the thumbnail only.
5. Save remains disabled until the edited values differ from the originals.
6. Cancel returns to Slot Actions without mutation.
7. Save performs one shared appearance-update operation.
8. On success, return to Current Slots and refresh only the Selected Area.
9. On validation or mutation failure, retain the editor values and allow Save retry or Cancel.
10. If mutation succeeds but synchronization fails, retain the editor and offer `Retry Sync`; the retry synchronizes only and does not rewrite appearance fields.

Cancel guarantees no mutation only before a Save has successfully written appearance fields. A synchronization failure is explicitly a partial success: the actor data has changed even though the visual synchronization did not complete.

## Architecture

```text
OfficialMenuFrameworkAdapter          Bridge::handleUpdateTattoo
  intent + presentation                         Prisma JSON
             |                                      |
             v                                      |
NativeSlotWorkflowModel                            |
  edit state + deterministic transitions           |
             |                                      |
             v                                      v
NativeSlotWorkflowRuntime --------------> SlaveTatsService
  SKSE task scheduling                    validation + typed contract
                                                   |
                                                   v
                                            ITattooRuntime
                                                   |
                                                   v
                                           SlaveTatsRuntime
                                  actor lookup + handle revalidation
                                  JContainers mutation + synchronize
```

The native adapter owns rendering only. The workflow model owns copyable editor state and generates typed tickets. The workflow runtime schedules service calls on the SKSE task interface and publishes completions. `SlaveTatsService` validates transport-independent inputs. `SlaveTatsRuntime` owns all actor, SlaveTatsNG, and JContainers behavior.

## Shared Core Contract

Core adds:

- `UpdateTattooAppearanceMode`: `updateAndSynchronize` or `synchronizeOnly`;
- `UpdateTattooAppearanceRequest`: actor form ID, session-local runtime handle, color, visible alpha, and mode;
- `UpdateTattooAppearanceSuccess`: actor form ID and runtime handle;
- `UpdateTattooAppearanceResult`: `std::expected<UpdateTattooAppearanceSuccess, ServiceError>`;
- `ServiceErrorCode::staleTattooHandle` for a handle that is zero, absent, or no longer belongs to the requested actor's applied tattoo collection;
- `ServiceErrorCode::updateFailed` for an appearance write failure.

`ITattooRuntime` and `SlaveTatsService` each gain `updateAppearance`. The service performs the same SlaveTatsNG and JContainers availability checks used by existing operations, then validates:

- nonzero actor form ID;
- nonzero runtime handle for `updateAndSynchronize`;
- color in inclusive `0x000000` through `0xFFFFFF`;
- finite visible alpha in inclusive `0.0F` through `1.0F`.

`synchronizeOnly` is valid only as an internal retry derived from a preceding synchronization failure. It retains the actor and handle identity in the typed request so stale workflow completions remain identifiable, but it performs no appearance write.

## Runtime Mutation and Stale-Handle Validation

For `updateAndSynchronize`, `SlaveTatsRuntime`:

1. Resolves `actorFormId` to `RE::Actor`.
2. Queries the actor's current applied SlaveTats collection.
3. Confirms that the exact numeric runtime handle appears in that collection.
4. Returns `staleTattooHandle` without writing or synchronizing if validation fails.
5. Writes `color` to the validated tattoo object.
6. Converts visible alpha through `toSlaveTatsInvertedAlpha()` and writes `invertedAlpha`.
7. Sets `.SlaveTats.updated` to `1` for the actor.
8. Calls `synchronize_tattoos(actor, false)` exactly once.

The runtime does not identify the target by slot, section, or name because those values can collide or become stale independently. The handle is authoritative only after membership revalidation against the requested actor.

For `synchronizeOnly`, the runtime resolves the actor, sets the updated flag, and calls `synchronize_tattoos` once. It does not re-query or rewrite the tattoo object because a failed synchronization follows a completed and validated mutation; the original handle may no longer be necessary for synchronization.

If synchronization fails after mutation, the result uses `synchronizeFailed` and states that appearance changed but synchronization failed. No rollback is attempted because restoring two JContainers fields would not guarantee restoration of downstream overlay state.

## Native Workflow Model

`SlotWorkflowScreen` gains `editAppearance` and `savingAppearance`.

The model retains an edit session containing:

- actor form ID, Selected Area, slot, and copied runtime handle;
- texture path for thumbnail reuse;
- original color and visible alpha;
- edited color and visible alpha;
- whether the next operation is a full update or synchronization-only retry.

Only a selected `SlotOccupancy::slaveTats` slot with tattoo metadata and a nonzero runtime handle may enter Edit Appearance. Empty and external slots cannot construct an edit session.

Transitions are deterministic:

```text
SlotActions --Edit Appearance--> EditAppearance
EditAppearance --Cancel--------> SlotActions
EditAppearance --Save----------> SavingAppearance
SavingAppearance --success-----> CurrentSlots + refresh Selected Area
SavingAppearance --write error--> EditAppearance + Save retry
SavingAppearance --sync error---> EditAppearance + Retry Sync
EditAppearance --Retry Sync-----> SavingAppearance
```

Color is clamped to the supported RGB range and alpha to the visible range when editor state changes. Dirty state compares the normalized edited pair with the immutable original pair. Save is enabled only when dirty and no operation is active. Retry Sync replaces Save after `synchronizeFailed`; it remains available even though the edited values now describe the partially committed appearance.

Generation tokens follow the existing query/apply/remove pattern. Completions from an obsolete edit session are ignored. A successful update clears the edit session, returns to Current Slots, and schedules a query for the Selected Area only.

## Game-Thread Coordination

`NativeSlotWorkflowRuntime` gains a typed appearance operation and ticket. Its existing single-in-flight guard covers query, apply, remove, and appearance update work. Scheduler rejection and unexpected exceptions become `updateFailed` for a full update or `synchronizeFailed` for a synchronization-only retry.

The workflow model decides the retry mode from the previous typed error. The runtime coordinator does not inspect error message strings and does not perform hidden automatic retries.

## Native UI

Slot Actions keeps Back, Replace, and Remove and adds `Edit Appearance` for SlaveTats-managed slots.

The new screen reuses or extracts presentation-neutral helpers already used by Preview:

- `0xRRGGBB` to and from ImGui color components;
- `ColorEdit3`;
- the `0.0F` to `1.0F` Alpha slider;
- tinted thumbnail rendering using edited color and alpha;
- a helper that determines whether Save is enabled.

The adapter submits edits to the model and renders its state. It never reads or writes a runtime handle through JContainers. While Saving Appearance is active, controls and navigation that could submit duplicate work are disabled.

## Prisma Compatibility

`Bridge::handleUpdateTattoo` constructs an `UpdateTattooAppearanceRequest` in `updateAndSynchronize` mode and calls `SlaveTatsService::updateAppearance` instead of writing JContainers directly.

The existing request fields remain unchanged:

```json
{
  "action": "updateTattoo",
  "actorId": 20,
  "tattooHandle": 123,
  "color": 16777215,
  "alpha": 1.0
}
```

Success remains exactly:

```json
{"type":"success","action":"updateTattoo"}
```

Failure remains `{"type":"error","message":"..."}` with JSON escaping. Prisma does not expose the synchronization-only mode and does not receive new structured error fields.

## Error Handling

- Missing SlaveTatsNG or JContainers: reject before runtime mutation.
- Missing actor: retain the editor and permit return to Slot Actions.
- Invalid color or alpha: reject before runtime mutation.
- Zero, stale, or foreign handle: show a refresh-oriented error and perform no mutation or synchronization.
- Appearance write failure: retain edited values and permit full Save retry.
- Synchronization failure after mutation: label the partial success clearly and offer synchronization-only retry.
- Synchronization retry failure: retain Retry Sync without rewriting appearance.
- Scheduler failure or exception: convert to the stable error appropriate to the requested mode; no exception crosses the render callback.

## Testing

### Core Service

- Dependency readiness failures stop before runtime calls.
- Zero actor and handle are rejected where applicable.
- Out-of-range color, non-finite alpha, and out-of-range alpha are rejected.
- Inclusive color and alpha boundaries are forwarded unchanged.
- Full update and synchronization-only modes reach the runtime unchanged.

### Runtime Seam

- Missing actor returns `actorNotFound` without mutation.
- Zero, absent, and foreign handles return `staleTattooHandle` without mutation or synchronization.
- A valid handle writes the requested RGB color and converted inverted alpha.
- Successful mutation marks the actor updated and synchronizes once.
- Synchronization failure reports partial success with `synchronizeFailed`.
- Synchronization-only retry performs no appearance write and synchronizes once.

### Native Workflow Model and Runtime

- Only a SlaveTats-managed Current Slot can enter Edit Appearance.
- Original and edited values initialize from the Slot Snapshot.
- Local edits change presentation state without emitting a ticket.
- Save enablement follows normalized dirty state.
- Cancel before Save emits no mutation and returns to Slot Actions.
- Duplicate Save input is ignored while saving.
- Successful Save returns to Current Slots and refreshes only the Selected Area.
- Validation or write failure retains editor values for retry.
- Synchronization failure switches to synchronization-only retry.
- Retry Sync never emits a second full mutation.
- Stale generations cannot overwrite current state.
- Scheduler rejection and exceptions clear the in-flight guard and publish stable errors.

### Adapter and Prisma

- RGB conversion preserves channel order.
- The tinted thumbnail uses edited color and alpha.
- Save is disabled for unchanged values and during submission.
- Retry Sync replaces Save after a synchronization failure.
- Prisma success JSON remains exactly compatible.
- Prisma errors retain the existing JSON envelope and escaping.

## In-Game Acceptance

1. F1 opens the native workflow and F8 PrismaUI remains functional.
2. An occupied SlaveTats Current Slot exposes Edit Appearance.
3. Color and Alpha initialize to the slot's current values.
4. Editing updates the thumbnail immediately without changing the actor.
5. Save remains disabled until at least one value changes.
6. Cancel before Save does not mutate the actor.
7. Save changes only color and alpha for the selected tattoo and slot.
8. Successful Save returns to Current Slots and refreshes only the Selected Area.
9. A stale handle shows an error and never edits another tattoo.
10. A synchronization failure can be retried without repeating the appearance write.
11. Prisma `updateTattoo` still accepts and returns its existing JSON contract.

## Implementation Boundaries

The change is expected to touch the typed Core models and service, runtime interface and adapter, native workflow model and coordinator, native adapter helpers/rendering, Bridge routing, and their existing test targets. It must not add native-only JContainers logic, call `applyToSlot` to edit appearance, retire Prisma behavior, or refactor unrelated mutation paths.

Implementation proceeds test-first at each seam. Before any implementation commit or deployment proposal, Debug and Release builds, their complete CTest suites, `git diff --check`, scoped diff review, and a secret scan must pass. Deployment requires a separate explicit approval.
