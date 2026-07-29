# SlaveTats UI

A PrismaUI overlay for SlaveTatsNG — browse, apply, and remove body tattoos from an in-game panel without opening any menus.

> Building from source or contributing? See [DEVELOPMENT.md](DEVELOPMENT.md) and [DEPLOY.md](DEPLOY.md).

---

## Requirements

All of the following must be installed and enabled **before** loading SlaveTats UI:

| Mod | Notes |
|-----|-------|
| [SKSE64](https://skse.silverlock.org/) | Match your Skyrim SE/AE build number exactly |
| [SlaveTatsNG](https://github.com/nopse0/SlaveTatsNG/tree/master) | Provides the tattoo application API |
| [JContainers SE](https://www.nexusmods.com/skyrimspecialedition/mods/16495) | Required by SlaveTatsNG for data storage |
| [PrismaUI](https://www.prismaui.dev/getting-started/introduction/) | CEF-based in-game browser overlay |
| One or more SlaveTats texture packs | Loose files or BSA — both are supported |

> **Recommended**: Use [Mod Organizer 2 (MO2)](https://github.com/ModOrganizer2/modorganizer) for installation. The plugin was designed and tested under MO2's virtual file system (usvfs).

---

## Installation

1. Download the latest `SlaveTatsUI.zip` from the releases page.
2. In MO2, click **Install a new mod from an archive** and select the zip.
3. Enable the mod. The mod structure is:
   ```
   SlaveTatsUI\
   ├── SKSE\Plugins\SlaveTatsUI.dll
   └── PrismaUI\views\SlaveTatsUI\index.html
   ```
4. Launch the game via **SKSE** (through MO2).

---

## Usage

The UI shows a **slot grid** grouped by body area (BODY / FACE / HANDS / FEET). Each slot is either empty (green), occupied (blue), or used by another mod's overlay (amber, read-only).

| Action | How |
|--------|-----|
| Open / close UI | Press **F8** in-game (configurable — see [Changing the hotkey](#changing-the-hotkey) below) |
| Switch actor | **Actor** dropdown in the header; click **&#8635;** to rescan nearby NPCs |
| Apply a tattoo to an empty slot | Click a green (empty) slot → pick a tattoo from the browser (expand Section → Area, or **Show All** to search across areas) → adjust color/alpha → **Apply to Slot** |
| Edit an applied tattoo | Click a blue (occupied) slot → adjust color/alpha → **Save** |
| Replace an applied tattoo | Open the slot's edit view → **Replace** → pick a new tattoo from the browser |
| Remove a tattoo | Open the slot's edit view → **Remove** → confirm |
| Filter the tattoo browser | Type in the search bar (filters by name/section) while the browser view is open |
| Sync visuals | Click **Sync** button (top-right) if tattoos appear out of sync |

### Changing the hotkey

The F8 toggle is configurable via `SlaveTatsUI.json`, created on first run at:
```
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\SlaveTatsUI.json
```
Edit the `"hotkey"` field to any of `F1`-`F12`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `TILDE`, `BACKSLASH`, `NUMPAD0`-`NUMPAD9`, or a raw DIK scancode integer, then restart the game.

### Thumbnails

- Thumbnails (128×128) load in the background — no game stutter.
- Textures are cached to disk after first load. Cache location:
  ```
  %USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\SlaveTatsUI\thumbcache\
  ```
- First load of a large pack may take a few seconds per tattoo. Subsequent game sessions load instantly from cache.
- Both loose `.dds` files and BSA-packed textures are supported (BC1, BC3, BC5, BC7, and uncompressed formats).

---

## Compatibility

- **Skyrim SE** (1.5.97) and **AE** (1.6.x) — depends on which SKSE64 and CommonLibSSE-NG build the DLL was compiled against.
- The UI panel appears on the right side of the screen (520px wide, draggable) and does not obstruct the left-side view of your character.
- Compatible with any SlaveTats texture pack that follows the standard path:
  `textures\actors\character\slavetats\<section>\*.dds`

---

## Troubleshooting

**UI does not open on F8**
- Confirm SKSE is running (check `My Games\…\SKSE\skse.log`).
- Confirm PrismaUI is installed and its own DLL is loading.
- Check `SKSE\Plugins\SlaveTatsUI.log` for errors.

**"JContainers Not Ready"**
- JContainers SE must load before SlaveTatsNG. Ensure load order is correct.
- The plugin retries internally; try pressing Refresh in the Available tab after a few seconds.

**Available tab shows nothing**
- Click **Refresh** — the plugin queries SlaveTatsNG on demand.
- Ensure at least one SlaveTats texture pack is installed.

**Thumbnail shows ⚠**
- Texture not found in loose files or BSA. Verify the texture pack is installed and enabled in MO2.
- Check `SlaveTatsUI.log` for the exact path the plugin attempted.

**Apply / Remove has no effect**
- SlaveTatsNG requires the actor to be loaded in-scene. Test on Player (FormID 0x14) while in first/third person.

---

## Log Location

```
%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\Plugins\SlaveTatsUI.log
```

Reference Links:
- SlaveTatsNG : https://www.loverslab.com/files/file/35989-slavetatsng (loverslab)
- SlaveTatsNG : https://github.com/nopse0/SlaveTatsNG/tree/master (github)
- SlaveTatsGUI: https://github.com/nopse0/SlaveTatsGUI
- PrismaUI: https://www.prismaui.dev/getting-started/introduction/
- SkyUI : https://github.com/doodlum/SkyUI-Community/
- Racemenu : https://www.nexusmods.com/skyrimspecialedition/mods/19080