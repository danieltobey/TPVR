# 01 First-person camera

**Status:** ✅ Verified (steady camera). 🟡 Built: View Mode choice and "Camera Follows Animations" naming; the steady camera together with upstream's Full Body (merged 2026-10-15).

**Upstream:** fork-only. JoeyAW merged PR #10 on 2026-09-29 without this steady camera (removed at their request); Daniel keeps it in his build (2026-10-01). Leave it out of future upstream PRs.

## Problem
In first person the camera rode Link's animated body: it leaned forward along his facing when running, swung when he turned, popped when the stick was pressed or released, and overshot on starts and stops (it predicted one tick ahead). Swimming, crawling, vines and dialogue followed the animated head, so every stroke or gesture moved the view.

## Required behaviour
1. By default the first-person view is anchored to Link's physics position (`current.pos`) plus a calibrated eye height; nothing depends on his facing, stick input or animation.
2. The anchor position is interpolated between the last two game ticks, never extrapolated, so it stays in step with the interpolated world and never overshoots.
3. While swimming, climbing vines, crawling, walking underwater and in dialogue, the anchor uses `current.pos` horizontally and a low-pass-filtered stance height (~0.3 s), easing between stances instead of bobbing.
4. Horse/canoe/board riding, magnet-boots wall/ceiling walking, clawshot flight/hanging and scripted cutscenes keep their existing anchors.
5. View Mode is a single choice: first person, the game's original third-person camera, or upstream's Giant Screen (the flat game on a large virtual screen, added 2026-10). Giant Screen keeps the first/third-person choice, so leaving it returns to that perspective.
6. The steady behaviour can be turned off, restoring the original animation-following camera.

## Settings
| Setting | Values | Default |
|---|---|---|
| View Mode | First Person / Third Person / Giant Screen | First Person |
| Camera Follows Animations (first person only) | On / Off | Off (steady camera) |

## Verification
- Stand still: no idle sway. Sprint from standstill and release: no lurch or overshoot. 180° skid turn: no sideways swing. ✅
- Crawl, swim, talk to an NPC: height eases between stances, no bobbing. ✅
- View Mode is always exactly one of its three choices; Camera Follows Animations is greyed out in third person and Giant Screen. 🟡
- With Full Body on (upstream default; the body always follows the headset since upstream dropped Body Follows Headset, 2026-10-14), the steady camera behaves as above: no sway, and the body stays under the view. 🟡

## Notes
Upstream replaced Show Link's Body with Full Body (2026-10-14: body without the head, arms reaching to the controllers, on by default); the old setting is no longer read. The old neck-clearance nudge is only applied when Camera Follows Animations is on.
