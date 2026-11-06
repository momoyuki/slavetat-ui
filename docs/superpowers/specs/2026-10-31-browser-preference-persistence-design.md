# Browser Preference Persistence Design

The user-approved 2026-10-31 Beta 2 follow-up persists Thumbnail BG and the last Player Browser view across process restarts using the existing shared PluginConfigFile. It supersedes the session-only persistence prohibition in the 2026-10-15 camera spec. No new dependencies, tattoo mutations, NPC camera control, or release/deployment are included.

## Background

Store `thumbnailBackground` as `black` or `white`; default black. Load at plugin initialization. Save a selection before changing the displayed preference. Failed writes retain the previous displayed mode and show an error. Rendering continues to draw the background separately from cached DDS textures.

## Player view

Store `browserCameraViews` version 1 with independent optional `left` and `right` poses. Each pose contains only cameraOffset, pivotOffset, yaw and pitch. These are translation-relative to the current loaded Player root, with world-axis offsets and compass-fixed orientation, matching accepted session recall. Turning the Player intentionally does not rotate the saved view; this avoids changing the established framing convention. Never save absolute world positions, Actor IDs, or pointers.

Accept only three-component finite offsets bounded to +/-4096 units, camera-to-pivot distance 1..4096 units, yaw +/-pi and pitch +/-pi/2. Normalize captured yaw before saving; reject invalid configuration rather than clamp it. Missing data uses initial framing; a malformed side does not discard a valid opposite side. Only the bounded camera preference subtree is validated; unrelated configuration remains governed by the existing shared file contract. Oversized arrays and unexpected pose members are rejected.

Capture only the final rendered camera during a normally closing, still-owned Player session. Preserve the existing exact normal-camera restoration and SmoothCam release. Save once per successful close, never per frame. A write failure keeps the valid in-process view and reports a warning; it never prevents restoration.

PreLoadGame/NewGame invalidate queued motion and live ownership without recording the old session or deleting saved preferences. Store initialization captures no world data. A launcher action `Reset saved Player views` removes both poses, preserves Initial Pan/framing settings, invalidates queued/live camera intent before saving, and advances a generation token so an old session cannot recreate them. The next opening uses initial framing. Failed clear retains preferences and reports an error.

## Verification

Regression tests cover restart round trips, independent sides, key preservation with BG/cache settings, failed writes/clear rollback, missing/malformed/oversized camera subtree input, nonfinite/bounded poses, lifecycle invalidation without capture, and clear generation protection. Run complete Debug and Release builds/CTest sequentially. Engine rendering, SmoothCam continuity, and restart appearance require the user's combined in-game test later.
