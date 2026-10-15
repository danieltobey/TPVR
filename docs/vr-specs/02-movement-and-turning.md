# 02 Movement & turning

**Status:** ✅ Verified: Smooth Movement, horse turn-follow. 🟡 Built: horse steering deadzone and 0.4 s smoothing; Face Z-Target (upstream's implementation, adopted 2026-10-15).

## Problem
- Link's speed blended toward his planted foot's animation during starts and stops, so movement pulsed with each footstep (felt as a stutter with the camera on his position).
- Starting from a standstill after looking around, he pivoted on the spot or curved round from his old facing.
- On Epona the view stayed put while the horse turned underneath, and her turns start and stop abruptly with stick input.
- While Z-targeting, strafing around a target meant constantly turning to keep it in view.

## Required behaviour
1. **Smooth Movement** (first person): move at the plain speed ramp (+1.9/tick up, −2.2/tick down), with no footstep-sync blend; and on starting from a standstill, face the push direction (stick + head yaw) immediately. Ground movement only; Z-targeting, swimming, vines, riding, magnet boots and events are unchanged.
2. **Turn View With Horse**: while riding, the view rotates by the horse's change in facing, preserving the look direction relative to the horse and never snapping to horse-forward. The follow eases in and out (exponential, 0.4 s).
3. **Horse steering deadzone** (VR): stick deviations within 20° of the horse's facing steer straight; beyond that the angle ramps up from zero (remapped, no step at the threshold).
4. **Face Z-Target** (upstream's "Z-Target Camera Focus", `game.vrZTargetCameraFocus`, adopted 2026-10-15 in place of our own Z-Target Lock View, which did the same and was removed): in first person, on lock-on, switching targets, or a jump in the target's bearing, snap the view yaw to face the target; while held, rotate the view by the change in the target's bearing, smoothed (exponential, 0.2 s), so it keeps its place in view. A target closer than 40 units holds the view still. Head look stays free; yaw only. In third person the same setting lets the view follow the game's Z-target camera swing-in.
5. All view rotation is yaw-only; nothing tilts the view without head movement.

## Settings
| Setting | Values | Default |
|---|---|---|
| Smooth Movement (first person only) | On / Off | On |
| Turn View With Horse | On / Off | On |
| Face Z-Target | On / Off | On |
| Snap Turn / Smooth Turn Speed / Snap Turn Angle | existing | Off / 135 deg/s / 45 deg |

## Verification
- Per-tick movement log shows a clean ramp 1.9, 3.8 … 22.8 on start and 20.8 … 1.0 on stop. ✅
- Look 90–180° away while standing, push forward: walk straight where you look. ✅
- Ride Epona looking sideways and turn both ways: the look angle relative to the horse holds. ✅ Small stick deflections go straight; turns ramp gently. 🟡
- Z-target and circle: target stays put in view; head free; switching targets snaps. ✅ with our Z-Target Lock View (2026-09-29); re-check with upstream's Face Z-Target. 🟡
