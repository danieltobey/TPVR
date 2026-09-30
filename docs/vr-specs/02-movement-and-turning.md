# 02 Movement & turning

**Status:** ✅ Verified: Smooth Movement, Z-Target Lock View, horse turn-follow. 🟡 Built: horse steering deadzone and 0.4 s smoothing.

## Problem
- Link's speed blended toward his planted foot's animation during starts and stops, so movement pulsed with each footstep (felt as a stutter with the camera on his position).
- Starting from a standstill after looking around, he pivoted on the spot or curved round from his old facing.
- On Epona the view stayed put while the horse turned underneath, and her turns start and stop abruptly with stick input.
- While Z-targeting, strafing around a target meant constantly turning to keep it in view.

## Required behaviour
1. **Smooth Movement** (first person): move at the plain speed ramp (+1.9/tick up, −2.2/tick down), with no footstep-sync blend; and on starting from a standstill, face the push direction (stick + head yaw) immediately. Ground movement only; Z-targeting, swimming, vines, riding, magnet boots and events are unchanged.
2. **Turn View With Horse**: while riding, the view rotates by the horse's change in facing, preserving the look direction relative to the horse and never snapping to horse-forward. The follow eases in and out (exponential, 0.4 s).
3. **Horse steering deadzone** (VR): stick deviations within 20° of the horse's facing steer straight; beyond that the angle ramps up from zero (remapped, no step at the threshold).
4. **Z-Target Lock View** (first person): on lock-on, or switching targets, snap the view yaw to face the target; while held, rotate the view by the change in the target's bearing so it keeps its place in view. Head look stays free; yaw only.
5. All view rotation is yaw-only; nothing tilts the view without head movement.

## Settings
| Setting | Values | Default |
|---|---|---|
| Smooth Movement (first person only) | On / Off | On |
| Turn View With Horse | On / Off | On |
| Z-Target Lock View | On / Off | On |
| Snap Turn / Smooth Turn Speed / Snap Turn Angle | existing | Off / 135 deg/s / 45 deg |

## Verification
- Per-tick movement log shows a clean ramp 1.9, 3.8 … 22.8 on start and 20.8 … 1.0 on stop. ✅
- Look 90–180° away while standing, push forward: walk straight where you look. ✅
- Ride Epona looking sideways and turn both ways: the look angle relative to the horse holds. ✅ Small stick deflections go straight; turns ramp gently. 🟡
- Z-target and circle: target stays put in view; head free; switching targets snaps. ✅
