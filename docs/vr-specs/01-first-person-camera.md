# 01 First-person camera

**Status:** ✅ Verified (steady camera). 🟡 Built: Perspective choice and "Camera Follows Animations" naming.

## Problem
In first person the camera rode Link's animated body: it leaned forward along his facing when running, swung when he turned, popped when the stick was pressed or released, and overshot on starts and stops (it predicted one tick ahead). Swimming, crawling, vines and dialogue followed the animated head, so every stroke or gesture moved the view.

## Required behaviour
1. By default the first-person view is anchored to Link's physics position (`current.pos`) plus a calibrated eye height; nothing depends on his facing, stick input or animation.
2. The anchor position is interpolated between the last two game ticks, never extrapolated, so it stays in step with the interpolated world and never overshoots.
3. While swimming, climbing vines, crawling, walking underwater and in dialogue, the anchor uses `current.pos` horizontally and a low-pass-filtered stance height (~0.3 s), easing between stances instead of bobbing.
4. Horse/canoe/board riding, magnet-boots wall/ceiling walking, clawshot flight/hanging and scripted cutscenes keep their existing anchors.
5. Perspective is a single either/or choice: first person or the game's original third-person camera.
6. The steady behaviour can be turned off, restoring the original animation-following camera.

## Settings
| Setting | Values | Default |
|---|---|---|
| Perspective | First Person / Third Person | First Person |
| Camera Follows Animations (first person only) | On / Off | Off (steady camera) |

## Verification
- Stand still: no idle sway. Sprint from standstill and release: no lurch or overshoot. 180° skid turn: no sideways swing. ✅
- Crawl, swim, talk to an NPC: height eases between stances, no bobbing. ✅
- Perspective can't be first and third person at once; Camera Follows Animations is greyed out in third person. 🟡

## Notes
Show Link's Body is expected to be off; the old neck-clearance nudge is only applied when Camera Follows Animations is on.
