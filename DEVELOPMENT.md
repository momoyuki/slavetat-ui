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
                                         ┌── queryAvailable → handleQueryAvailable (game thread)
                                         ├── queryApplied   → handleQueryApplied   (game thread)
                                         ├── addTattoo      → handleAddTattoo      (game thread)
                                         ├── removeTattoo   → handleRemoveTattoo   (game thread)
                                         ├── syncTattoos    → handleSyncTattoos    (game thread)
                                         └── getTexture     → std::thread → handleGetTexture (bg)

slavetatsOnData(JSON) ◄─── PrismaUI ──── sendToUI(json)
```

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

| Tool | Version |
|------|---------|
| Visual Studio 2022 | with "Desktop development with C++" workload |
| CMake | 3.21 or later |
| vcpkg | integrated with Visual Studio or standalone |
| Windows SDK | 10.0.19041.0 or later (for DirectXTex COM) |

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

Run `vcpkg install` in the project root, or let CMake's toolchain file handle it automatically.

---

## Building

```powershell
# Configure
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md

# Build (debug)
cmake --build build --config Debug

# Build (release)
cmake --build build --config Release
```

Output DLL: `build\debug\SlaveTatsUI.dll` or `build\release\SlaveTatsUI.dll`

---

## Deployment (MO2)

```powershell
$mod = "D:\Modding\SKYRIM-MOD\mods\SlaveTatsUI"

# DLL
Copy-Item "build\debug\SlaveTatsUI.dll" "$mod\SKSE\Plugins\SlaveTatsUI.dll" -Force

# UI
Copy-Item "view\index.html" "$mod\PrismaUI\views\SlaveTatsUI\index.html" -Force
```

> **Never** place files directly in the Skyrim data directory. MO2's usvfs virtualises them at runtime.

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

| type | Fields | Trigger |
|------|--------|---------|
| `ready` | — | Plugin fully initialised |
| `available` | `tattoos[]` | Response to `queryAvailable` |
| `applied` | `tattoos[]` | Response to `queryApplied` |
| `success` | `action`, `section`, `name` | Add/remove/sync succeeded |
| `error` | `message` | Any error |
| `texture` | `path`, `w`, `h`, `data` (base64 RGBA) | Response to `getTexture` |
| `textureError` | `path` | Texture not found |

### Message types (JS → C++)

| action | Fields |
|--------|--------|
| `queryAvailable` | `domain` |
| `queryApplied` | `actorId` (hex FormID) |
| `addTattoo` | `actorId`, `section`, `name`, `color`, `alpha` |
| `removeTattoo` | `actorId`, `section`, `name` |
| `syncTattoos` | `actorId` |
| `getTexture` | `path` (relative to `textures\actors\character\slavetats\`) |
| `toggleUI` | — |
