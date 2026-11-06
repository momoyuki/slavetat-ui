# Thumbnail clarity implementation

User approved implementation on 2026-11-02 while another chat owns PR #23.
Scope: vNext.5 visual thumbnail quality follow-up. Preserve unrelated preference
persistence work, original tattoo textures, theme and cache budgets.

## Sequence and evidence

- [x] Read roadmap, context, gallery spec, cache-limit spec and current source/tests.
- [x] Reproduce lost detail: a 512px BC3 fixture with 32 alternating motif stripes
  produces only 16 color transitions under full-canvas reduction (Debug red test).
- [x] Crop padded, bounded source alpha before BC3 reduction and BC7 resampling.
- [x] Use alpha-weighted BC3 colors and premultiplied BC7 filtering.
- [x] Version disk key to v6. Tag generated DDS reserved header words with STUITH6
  so reload does not apply a second visible-alpha crop. Keep dimensions <=192.
- [ ] Verify motif detail in BC3/BC7, direct GPU upload and disk roundtrip.
- [ ] Run full Debug and Release builds/tests and diff checks.
- [ ] Review pixel results and performance limits before deploy approval.
- [ ] In-game rebuild, fine-line/color comparison, black/white BG, Browser/Slots,
  scroll speed and memory check after separately approved deployment.

## Decisions and limits

The previous BC7 mip policy cannot preserve small motifs: determine bounds from
level zero, bounded to 16M pixels, then crop and resize. This uses transient CPU
memory during first generation, not retained VRAM; prebuild remains recommended.
Do not enlarge a crop below192 before upload. Keep the source-relative eightfold
crop cap, padding and empty-alpha full-image fallback. Cached output is already
framed and uses full UVs to avoid exceeding that cap on reload.
No new dependency, actor workflow or localization change. No commit/deploy yet.

Original planning reference:
C:/Users/mskmktx/.codex/visualizations/2026/10/17/01a1499b-16bc-74c1-9643-336e2f2f7645/thumbnail-clarity-plan-2026-11-02.md
