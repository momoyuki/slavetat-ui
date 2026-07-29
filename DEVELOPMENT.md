# SlaveTats UI — Development Guide

## Architecture

```
Skyrim / SKSE
│
├── SlaveTatsNG.dll        ← tattoo data + actor manipulation API
├── JContainers64.dll      ← JSON object store (required by SlaveTatsNG)
├── PrismaUI.dll           ← CEF in-game browser overlay
│
└── SlaveTatsUI.dll        ← THIS PLUGIN
       │
       ├── main.cpp        SKSE plugin entry, hotkey (F8), message routing
       ├── Bridge.h/.cpp   JS↔C++ command dispatcher + texture pipeline
       └── pch.h           Precompiled header
```

### Communication Flow

```
HTML (index.html)                         Bridge.cpp
─────────────────                         ──────────
slavetatsCmd(JSON)  ──── PrismaUI ──────► onJSCommand()
                                              │
                                         dispatch by action:
                                         ┌── toggleUI        → toggleUI()                (UI thread)
                                         ├── queryActors     → handleQueryActors          (game thread)
                                         ├── queryAvailable  → handleQueryAvailable       (game thread)
                                         ├── querySlots      → handleQuerySlots           (game thread)
                                         ├── queryAllSlots   → handleQueryAllSlots        (game thread, loops 4 areas)
                                         ├── applyToSlot     → handleApplyToSlot          (game thread)
                                         ├── removeFromSlot  → handleRemoveFromSlot       (game thread)
                                         ├── updateTattoo    → handleUpdateTattoo         (game thread)
                                         ├── syncTattoos     → handleSyncTattoos          (game thread)
                                         ├── getTexture      → std::thread → handleGetTexture (bg)
                                         │
                                         │  legacy actions — still handled, but the current
                                         │  index.html UI does not call these anymore:
                                         ├── queryApplied    → handleQueryApplied         (game thread)
                                         ├── addTattoo       → handleAddTattoo            (game thread)
                                         └── removeTattoo    → handleRemoveTattoo         (game thread)

slavetatsOnData(JSON) ◄─── PrismaUI ──── sendToUI(json)
```

Current UI (`view/index.html`) is slot-based: it queries all slots for BODY/FACE/HANDS/FEET up front (`queryAllSlots`) and renders a grid per area. Clicking a free slot opens the tattoo browser (`applyToSlot`); clicking an occupied slot opens the edit view (`updateTattoo` / `removeFromSlot`). The older `queryApplied`/`addTattoo`/`removeTattoo` actions and the `"applied"` response type predate this slot model — Bridge.cpp still implements them, but `slavetatsOnData` in `index.html` has no `case 'applied':`, so responses to `queryApplied` are silently dropped by the current front-end. Treat those three as legacy/available-for-reuse, not part of the active contract.

### Texture Pipeline

```
handleGetTexture (background thread)
  │
  ├─ 1. Check disk cache (.rgba file) → hit → sendRGBAToUI → done
  │
  ├─ 2. LoadFromDDSFile (loose file via Win32)
  │         └── success → sendDecodedTexture → cache + sendRGBAToUI → done
  │
  └─ 3. BSA fallback (game thread via SKSE TaskInterface)
              └── BSResourceNiBinaryStream
                    └── std::thread → LoadFromDDSMemory → sendDecodedTexture
```

`sendDecodedTexture`:
- Decompresses BC1/BC3/BC5/BC7 → RGBA8 via DirectXTex
- Resizes mip0 to 128×128 via DirectXTex
- Strips rowPitch padding → raw RGBA bytes
- Writes `.rgba` cache file
- Base64-encodes → `sendToUI({type:"texture", path, w, h, data})`

---

## Build Requirements

| Tool | Version | Notes |
|------|---------|-------|
| Visual Studio 2022 | any edition, "Desktop development with C++" workload | provides `vcvarsall.bat` + `vswhere.exe` |
| CMake | 3.21 or later | |
| Ninja | latest | CMake presets use the Ninja generator |
| vcpkg | standalone clone | set `VCPKG_ROOT` to point at it (see below) |
| Windows SDK | 10.0.19041.0 or later | for DirectXTex COM |

---

## vcpkg Dependencies

Declared in [vcpkg.json](vcpkg.json):

```json
{
  "name": "slavetats-ui",
  "version": "0.1.0",
  "dependencies": [
    "commonlibsse-ng",
    "nlohmann-json",
    "directxtex"
  ]
}
```

`commonlibsse-ng` is pulled from the overlay registry declared in [vcpkg-configuration.json](vcpkg-configuration.json) (colorglass' vcpkg fork), not the main vcpkg registry. The custom triplet `x64-windows-skse` ([cmake/x64-windows-skse.cmake](cmake/x64-windows-skse.cmake)) links SKSE/game-engine ports dynamically and everything else statically — this is what CommonLibSSE-NG plugins expect.

Dependencies are resolved automatically by CMake's vcpkg toolchain integration during configure — no manual `vcpkg install` step needed.

---

## Building

### One-time setup

```powershell
$env:VCPKG_ROOT = "C:\path\to\your\vcpkg"   # set once, e.g. in your PowerShell profile
```

### Build

```powershell
.\build.ps1 -Config debug    # or -Config release (default)
```

`build.ps1`:
1. Locates your Visual Studio 2022 install via `vswhere.exe` (works with any edition, not just Build Tools) and imports the MSVC x64 environment.
2. Runs `cmake --preset build-$Config` (see [CMakePresets.json](CMakePresets.json)) to configure with the vcpkg toolchain and `x64-windows-skse` triplet.
3. Runs `cmake --build build/$Config`.

Equivalent manual steps, if you'd rather not use the script:

```powershell
cmake --preset build-debug      # or build-release
cmake --build build/debug       # or build/release
```

Output DLL: `build\debug\SlaveTatsUI.dll` or `build\release\SlaveTatsUI.dll`

---

## Deployment

See [DEPLOY.md](DEPLOY.md) for copying a build into an MO2 profile for in-game testing.

---

## Key Technical Notes

### `interface` macro conflict

DirectXTex pulls in COM headers which define:
```c
#define interface __interface
```
This breaks `slavetats::interface::Interface`. Both `pch.h` and `Bridge.h` must `#undef interface` after including `<DirectXTex.h>`:
```cpp
#include <DirectXTex.h>
#ifdef interface
#  undef interface
#endif
```

### SlaveTatsNG return values

`slavetats::interface::simple_add_tattoo` and `simple_remove_tattoo` return `fail_t` (`bool`), but **return `true` even on success** (suspected: returns slot number cast to bool, or ForceSync sub-step fails while visual succeeds). Bridge treats any return value as success and logs a warning if `true`:
```cpp
if (failed) logger::warn("...");
sendToUI(R"({"type":"success",...})");  // always
```

### JContainers plugin name

The SKSE plugin name is **`"JContainers64"`** (NOT `"JContainers"`). Register the messaging listener as:
```cpp
msg->RegisterListener("JContainers64", onJContainersMessage);
```

### BSA vs loose file visibility

MO2's usvfs hooks Win32 file I/O, so loose files are visible to `LoadFromDDSFile`. BSA contents are **not** visible via Win32 — they require `RE::BSResourceNiBinaryStream`, which must be called on the **game thread** (SKSE TaskInterface).

### Thumbnail disk cache

Cache files are raw binary `.rgba`:
```
[uint32 width] [uint32 height] [width × height × 4 bytes RGBA]
```
Location: `SKSE log dir / SlaveTatsUI / thumbcache / <sanitized_path>.rgba`

For loose files: invalidated by mtime comparison (source newer than cache → re-decode).
For BSA files: permanent (BSA contents don't change at runtime).

---

## File Structure

```
slavetat-ui\
├── CMakeLists.txt
├── vcpkg.json
├── README.md
├── DEVELOPMENT.md
├── DEPLOY.md
├── build.ps1
├── include\
│   ├── PrismaUI_API.h             PrismaUI C++ header
│   ├── SlaveTatsNG_Interface.h    SlaveTatsNG API
│   ├── jcontainers_mini.h         JContainers minimal binding
│   └── JContainers\
│       ├── jc_interface.h
│       └── jcontainers_constants.h
├── src\
│   ├── pch.h                      Precompiled header
│   ├── main.cpp                   SKSE plugin entry + hotkey
│   ├── Bridge.h
│   └── Bridge.cpp                 All game↔UI logic
└── view\
    └── index.html                 PrismaUI overlay (HTML/CSS/JS)
```

---

## PrismaUI Interop API

### C++ → JS

```cpp
// Send JSON string; JS receives it in slavetatsOnData(jsonStr)
void Bridge::sendToUI(const std::string& json) {
    m_prismaUI->InteropCall(m_view, json.c_str());
}
```

### JS → C++

```javascript
// JS side: call C++ handler
slavetatsCmd(JSON.stringify({ action: 'addTattoo', ... }));

// C++ registers via PrismaUI:
m_prismaUI->RegisterJSListener(m_view, "slavetatsCmd", onJSCommand);
```

### Message types (C++ → JS)

Handled by the `switch(d.type)` in `slavetatsOnData` ([view/index.html](view/index.html)):

| type | Fields | Trigger |
|------|--------|---------|
| `ready` | `apiVersion`, `apiOk` | View created; SlaveTatsNG API bound (or not) |
| `show` | — | UI opened via `toggleUI` (F8) |
| `available` | `domain`, `tattoos[]` | Response to `queryAvailable` |
| `actors` | `actors[]` (`id`, `name`, `isPlayer`) | Response to `queryActors` |
| `slots` | `area`, `maxSlots`, `slots[]` (`slot`, `occupied`, `external?`, `name?`, `section?`, `texture?`, `color?`, `alpha?`, `handle?`) | Response to `querySlots` / `queryAllSlots` (fired once per area) |
| `success` | `action`, plus action-specific fields (e.g. `slot`/`section`/`name` for `applyToSlot`, `area`/`slot` for `removeFromSlot`) | `applyToSlot` / `removeFromSlot` / `updateTattoo` / `syncTattoos` succeeded |
| `error` | `message` | Any error |
| `texture` | `path`, `w`, `h`, `data` (base64 **raw RGBA**, not an encoded image — decode via `atob`+`ImageData`+`createImageBitmap` and draw to `<canvas>`) | Response to `getTexture` |
| `textureError` | `path` | Texture not found / decode failed |

Legacy, still sent by Bridge.cpp but **not handled** by the current `index.html` (no matching `case`): `applied` (response to `queryApplied`).

### Message types (JS → C++)

Dispatched by `action` in `onJSCommand` ([src/Bridge.cpp](src/Bridge.cpp)):

| action | Fields | Used by current UI? |
|--------|--------|------|
| `toggleUI` | — | yes |
| `queryActors` | — | yes |
| `queryAvailable` | `domain` (default `"default"`) | yes |
| `querySlots` | `actorId`, `area` | via `queryAllSlots` |
| `queryAllSlots` | `actorId` | yes — queries BODY/FACE/HANDS/FEET |
| `applyToSlot` | `actorId`, `section`, `name`, `domain`, `slot`, `color`, `alpha` | yes |
| `removeFromSlot` | `actorId`, `area`, `slot` | yes |
| `updateTattoo` | `actorId`, `tattooHandle`, `color`, `alpha` | yes |
| `syncTattoos` | `actorId` | yes |
| `getTexture` | `path` (relative to `textures\actors\character\slavetats\`) | yes |
| `queryApplied` | `actorId` (hex FormID) | no — legacy |
| `addTattoo` | `actorId`, `section`, `name`, `color`, `alpha` | no — legacy |
| `removeTattoo` | `actorId`, `section`, `name` | no — legacy |

`actorId` defaults to `0x14` (player) server-side if omitted. All actor-scoped actions except `getTexture`/`queryActors`/`toggleUI` run on the game thread via `SKSE::GetTaskInterface()->AddTask(...)`; `getTexture` runs on a detached background thread (file I/O + DirectXTex decode).
