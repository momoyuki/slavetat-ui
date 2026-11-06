# Thumbnail Disk Cache Limit

Approved scope for the v1.8.0 Beta 2 follow-up: the SKSE launcher offers a
Thumbnail disk cache limit of 128, 256, 512, or 1024 MiB, defaulting to 256 MiB,
and Reset to default. Display used disk bytes against the applied limit.

A dedicated ThumbnailCacheSetting validates integer `thumbnailDiskCacheLimitMiB`
and persists through PluginConfigFile, preserving unrelated keys. Missing values
use 256; invalid values report failure and fall back to 256. No GPU cache budget,
thumbnail resolution, actor workflow, or pack texture changes are included.

Launcher actions schedule work through NativeThumbnailRuntime. The source mutex
serializes the cache budget change and oldest-file pruning with disk loads,
stores, builds and clears. No deletion or config write occurs during rendering.
Startup applies the stored budget through the same scheduler before browsing.
Limit changes, build and clear are mutually exclusive. Pending and failed states
are visible; selecting a value again retries a failed operation. On prune failure
the previous limit remains active and the config is untouched. On config save
failure the source attempts to restore the previous budget; files already evicted
remain disposable. If restoration also fails, failure remains visible and runtime
status reports the actual new budget. Selecting a limit again retries persistence.

Validation covers config types/ranges and preservation, immediate shrink/oldest
entry eviction, scheduler deferral, conflict guards, startup application and
failure/retry. Debug and Release full CTest suites run sequentially. In-game
launcher interaction and filesystem permission behavior need a manual smoke test.
