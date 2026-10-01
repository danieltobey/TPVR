# 06 Cutscenes

**Status:** ✅ Verified: facing, pan following, Window crop, TV mode (size, float, stereo), HUD unaffected by TV. 🟡 Built: TV Size / TV Distance settings in cm, TV Height.

## Problem
- Cutscene view direction came only from the headset plus accumulated stick turning, so whether you faced the action depended on how you'd turned in-game, and mid-shot camera pans weren't followed.
- The VR field of view (~100°) shows far beyond what cutscene cameras frame (~40–60°), revealing things scenes weren't made to show.
- Some scenes need looking up/down, which a head-locked presentation can avoid.

## Required behaviour
1. **Face Cutscene Camera**: on every camera-driven event (any event where the view follows the game camera), snap view yaw to the camera at the start and on each cut (≥25° yaw jump); when it ends, snap to Link's facing.
2. **Follow Cutscene Camera Turns**: within a shot, rotate the view yaw by the camera's pan each frame (requires 1).
3. **Cutscene View**:
   - *Full*: no crop.
   - *Window*: black out everything outside the original camera's frame (its fovy and aspect), as a world-locked frame around the cutscene camera eye. Frame size follows the camera's fov.
   - *TV*: every eye view is built from the cutscene camera (eye → centre, world up) with only the IPD offset kept, and the projection zoomed so the camera's fov fills a screen of fixed physical size (diagonal) at a fixed distance. The screen floats: it sits along a head-forward direction smoothed in real tracking space (menu-style damping, 0.08/frame), upright. It follows head turns and tilts as before; **TV Height** then shifts it up or down from that direction by a share of the screen's own height, so the shift stays the same relative to the screen whatever its size and distance.
4. The crop frame is drawn before the HUD, so text boxes are never covered. In TV mode the HUD, text boxes and menus are drawn with the normal head view and projection, unaffected by the TV view/zoom.

## Settings
| Setting | Values | Default |
|---|---|---|
| Cutscene View | Full / Window / TV | Window |
| TV Size (diagonal) | 100–2400 cm | 760 cm |
| TV Distance | 100–1000 cm | 500 cm |
| TV Height | −50% to +50% of screen height, steps of 5% | −20% |
| Face Cutscene Camera | On / Off | On |
| Follow Cutscene Camera Turns | On / Off | On |
| First-Person Cutscenes (Experimental) | On / Off | Off |

## Verification
- Each shot starts facing the action; pans are matched; after the scene you face Link's direction. ✅ (long cutscene not yet tried)
- Window: outside the frame is black, text boxes stay visible. ✅
- TV: constant screen size across close-ups and wide shots, trails fast head turns and settles, stays level, stereo comfortable, HUD unaffected. ✅
- TV Size / Distance change the screen as a real screen would. 🟡
- TV Height: the default sits visibly lower than before (centre 20% of the screen's height below where you look); −50% / +50% move it clearly down / up; 0% matches the old position; the TV still follows head turns and tilts and settles as before. 🟡 Host unit test for the offset angle. 🟡
