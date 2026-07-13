# SlaveTats UI — Deploy Guide (dev testing via MO2)

This covers copying a freshly built DLL + UI into a Mod Organizer 2 profile for
in-game testing. For build instructions see [DEVELOPMENT.md](DEVELOPMENT.md#building).
For end-user installation of a packaged release, see [README.md](README.md#installation).

---

## Prerequisites

- Build output exists: `build\debug\SlaveTatsUI.dll` or `build\release\SlaveTatsUI.dll`
  (run [`build.ps1`](build.ps1) first if not).
- MO2 profile with the load-order dependencies from
  [README.md](README.md#requirements) already installed and enabled:
  SKSE64, SlaveTatsNG, JContainers SE, PrismaUI, and at least one texture pack.
- A dedicated MO2 mod entry for this plugin (create an empty one via
  **MO2 → right-click mod list → All Mods → Create empty mod**, name it e.g. `SlaveTatsUI`).

---

## Expected mod folder layout

```
<MO2 mods dir>\SlaveTatsUI\
├── SKSE\Plugins\SlaveTatsUI.dll
└── PrismaUI\views\SlaveTatsUI\index.html
```

This mirrors the layout shipped in release zips (see
[README.md](README.md#installation)) — the same mod folder can be used for both
dev iteration and, once built in release config, for producing a distributable zip.

---

## Deploy

```powershell
$mod = "D:\Modding\SKYRIM-MOD\mods\SlaveTatsUI"   # adjust to your MO2 mods path
$config = "debug"                                  # or "release"

Copy-Item "build\$config\SlaveTatsUI.dll" "$mod\SKSE\Plugins\SlaveTatsUI.dll" -Force
Copy-Item "view\index.html" "$mod\PrismaUI\views\SlaveTatsUI\index.html" -Force
```

> **Never** place files directly in the Skyrim `Data` directory. MO2's usvfs
> virtualises the mods folder at runtime — copying into `Data` bypasses MO2's
> mod management (enable/disable, conflict resolution, uninstall) entirely.

`Copy-Item -Force` will fail with a file-in-use error if Skyrim/SKSE is still
running with the plugin loaded — close the game before redeploying the DLL.
`index.html` has no such lock since PrismaUI reloads it fresh each launch.

---

## Verify the deploy

1. Launch the game via **SKSE through MO2** (not a bare `SkyrimSE.exe` shortcut).
2. Check `<mod>\SKSE\Plugins\SlaveTatsUI.log` (visible in MO2's virtual `Data`
   dir at `SKSE\Plugins\SlaveTatsUI.log`) for the plugin's startup log lines —
   confirms the new DLL actually loaded, not a stale cached one.
3. Press **F8** in-game; the panel should open. If it doesn't, see
   [README.md — Troubleshooting](README.md#troubleshooting).

---

## Iterating during development

Rebuild + redeploy loop:

```powershell
.\build.ps1 -Config debug
Copy-Item "build\debug\SlaveTatsUI.dll" "$mod\SKSE\Plugins\SlaveTatsUI.dll" -Force
```

`index.html` changes take effect on next game launch without a rebuild — copy
it over the same way whenever [view/index.html](view/index.html) changes.
