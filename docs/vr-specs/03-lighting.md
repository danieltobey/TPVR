# 03 Lighting

**Status:** ✅ Verified: per-view lighting for characters and static objects, sun-glare dimming off, Sun/Moon fill light. 🟡 Built: Follow Look and Original modes not individually checked.

## Problem
- Characters and objects are drawn from frame-interpolation recordings. The replay re-aims each material's view-relative lights at the current view, but it ran once per frame against the invisible flatscreen chase camera, before the VR eye passes. NPC lighting turned with the head and swung as Link moved.
- `J3DMaterial::needsInterpCallBack()` only records animated / view-dependent-texture materials, so static lit models (signs, fences, props) were never replayed at all and kept chase-camera lighting.
- The outdoor fill light in `settingTevStruct_plightcol_plus()` is attached to the camera.
- Sun-glare darkening scaled the whole scene by the sun's position on the flatscreen camera's screen and its occlusion, pumping brightness up to ~80% as Link walked in and out of cover.

## Required behaviour
1. Replayed models are replayed once per VR view (eye pass or single-pass stereo pass) with that view installed; the flatscreen replay is skipped while VR renders, and runs for the flatscreen if VR doesn't render a frame.
2. In VR, any model whose materials carry baked view-relative lights is recorded for replay, not only animated ones. Always on.
3. The outdoor fill light comes from the selected source: the scene's global light (sun by day, moon by night, or the area light), from above and behind a ~1 s-smoothed look direction, or the original camera. Minimum elevation 25°.
4. Camera-eye-relative lighting (sun/moon placement, eye lights) uses the headset eye unless Original is selected.
5. Sun-glare scene darkening is disabled in VR. The lens flare is unaffected.

## Settings
| Setting | Values | Default |
|---|---|---|
| Lighting | Original / Sun/Moon / Follow Look | Sun/Moon |

## Verification
- Per-pass logging: object light setups in eye passes go from 0 to all of them. ✅
- NPC lighting stable under head turns and movement; the Ordon Ranch sign doesn't change with distance. ✅
- Walking in and out of shade no longer changes scene brightness. ✅
- A/B in central Ordon: 64.8 vs 65.0 FPS with vs without static-model relighting (GPU-bound). ✅
- Follow Look and Original each behave as described. 🟡
