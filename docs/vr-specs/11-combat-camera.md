# 11 Combat camera and third-person movement

**Status:** 🟡 Built (2026-10-01). Scripted headset run passed for entering and leaving combat (see Verification); the rest needs Daniel's in-headset check.

**Upstream:** candidate for a future upstream PR. Branch `feature/combat-camera` from `dev`, because it extends the Perspective choice on the regrouped settings page (specs 01 and 09), which upstream doesn't have yet.

## Problem
In first person, fights are hard to read: enemies flank you out of view and you can't see Link's sword swings or guard. Third person is better for fighting but worse for exploring. Players want first person for exploring and third person for fighting, without opening the settings menu each time.

Separately, in third person Link moves relative to the headset: "forward" on the stick is wherever your head points (`d_a_alink.cpp`, `mMoveAngle = mStickAngle + getHeadMoveAngleS()`, used for every VR view). Turning your head to look around while running changes Link's direction, which feels wrong when you're watching him from behind. This affects the existing Third Person choice and would affect the combat view too.

## Required behaviour
1. A new on/off setting, **Third Person in Combat**, in the View section under Perspective. While Perspective is First Person, turning it on switches to the third-person view (the same view as the Third Person choice, with Link's body shown) while Link is in combat, and back to first person afterwards. While Perspective is Third Person, it is greyed out (it has nothing to do).
2. **In combat** means any of these:
   - The game is playing its regular battle music (`Z2BGM_BATTLE_NORMAL`, `Z2BGM_BATTLE_TWILIGHT`), which the audio system starts when enemies near Link notice him (`Z2SoundObjMgr::searchEnemy()`).
   - The game is playing a boss, miniboss, face-off or horseback-battle track. These fights use their own music instead of the battle music. The full list of track IDs is kept in one table in the code (`kCombatBgmIds`, `vr_main.cpp`); it names the game's own enum values, so a wrong ID doesn't compile.
3. **Enter delay:** combat must last 0.5 s before the view switches, so an enemy that notices Link for a moment doesn't trigger it.
4. **Exit delay:** the view returns to first person 2 s after combat ends. If combat starts again within those 2 s, the view stays in third person. This is on top of the battle music's own fade-out.
5. **Fade:** each switch, in either direction, fades the view to black, swaps the camera while the view is fully black, then fades back in. That takes about 0.15 s out and 0.15 s in. Only the 3D view fades: the HUD and text boxes stay visible. When switching into third person, the view is turned while it is black so that straight ahead looks from the game camera at Link, so he is in front of you and "forward" moves him away from the camera.
6. **Aiming:** if Link aims an item while Z-targeting an enemy, he stays in third person, because the game aims at the target for him. If he aims without Z-targeting, the view switches to first person, as the original game does. This means the game's own first-person aim modes (bow, slingshot, boomerang, clawshot, Dominion Rod, Hawk, Ball and Chain, including the horse, canoe and underwater versions). The game's look-around view (C-up, and its horse, canoe, swimming and other versions) counts the same way. That switch is **immediate, with no fade and no delay**, so aiming isn't slowed down. When Link stops aiming, the view returns to third person immediately if still in combat. Otherwise it stays in first person.
7. **Exceptions that keep their current behaviour:** cutscenes, dialogue and door/chest transitions follow the existing rules in `isFirstPerson()`. Combat only decides the view during normal gameplay. If a cutscene starts mid-fight, it uses the existing cutscene rules, and afterwards the view follows the current combat state without a fade, because the cutscene's own fade covers the change.
8. Riding Epona and wolf form are included: a fight on horseback or as the wolf switches to third person like any other fight.
9. With Third Person in Combat off, First Person behaves exactly as it does now.

### Movement in third person
10. A new setting, **Third Person Movement**, chooses what the movement stick is relative to while the third-person view is in use during gameplay (the Third Person choice, or a combat switch): **Camera** (default) or **Headset** (today's behaviour). With Camera, the stick is relative to the camera, not the headset. "Camera" means the direction the third-person view faces before your head's own rotation is added: the smooth-turn yaw, which already follows the game camera when Turn With Game Camera is on. With the camera behind Link, pushing forward moves him away from you. Turning your head without moving the camera doesn't change his direction.
11. First person keeps headset-relative movement, unchanged. So does wolf form in first person (its view is head-based).
12. **Switching mid-run:** when Third Person Movement is Camera and the view switches while the stick is held, Link keeps going the way he was going. The old direction basis stays in use until the stick is released or moves more than 45° from where it was at the switch, then the new basis takes over. This stops a fade from swinging Link round mid-fight.
13. Z-targeting, riding (horse/canoe/board), swimming, and Iron Boots on walls keep their own movement rules. Rule 10 only replaces the basis that the stick angle is added to.

## Settings
| Setting | Values | Default |
|---|---|---|
| Perspective | First Person / Third Person | First Person (unchanged) |
| Third Person in Combat (first person only) | On / Off | Off |
| Third Person Movement (Third Person, or Third Person in Combat on) | Camera / Headset | Camera |

Stored as `game.vrThirdPersonInCombat` (bool). Its description: "Switches to third person during fights, then back. Default off." Off by default so the current behaviour doesn't change for anyone who doesn't turn it on. Third Person Movement is stored as `game.vrThirdPersonCameraMovement` (bool, on = Camera). Its description: "What the stick moves Link relative to in third person. Default Camera." It is greyed out unless Perspective is Third Person or Third Person in Combat is on. Default Camera, so the existing Third Person choice changes behaviour for everyone; Headset brings back the current behaviour.

The delays and fade times are constants (0.5 s, 2 s, 0.15 s), not settings. They can be made into settings later if they need tuning.

First-person settings that are greyed out in third person (Camera Follows Animations, and any others) stay available while Third Person in Combat is on, because the view is first person most of the time. Turn With Game Camera applies to the combat view too.

## Verification
- Walk up to a Bokoblin (Faron Woods, or Kakariko Gorge at night): the battle music starts, then the view fades to third person. Kill it: about 2 s after the music stops, the view fades back to first person. 🟡 Scripted run (`combat-camera.txt`, Faron Woods, 2026-10-01): battle music (sub BGM `0x100000f`) switched the view to third person behind Link facing the Bulblin, and back to first person after it was removed. Fade not yet seen by eye.
- Areas without battle music don't switch: in Ordon village the music stays the village track (`0x1000005`) with an enemy next to Link. Expected with the music-based rule.
- Walk past an enemy that notices Link for a moment and then loses him: the view doesn't switch, or doesn't flicker back and forth. 🔲
- In combat, Z-target an enemy and draw the bow: the view stays in third person and the arrow hits the target. 🔲
- In combat, draw the bow without Z-targeting: the view switches to first person right away, with no fade. Put the bow away: the view goes back to third person right away. 🔲
- Clawshot aim without Z-targeting in combat: first person, with no drift (the existing clawshot workaround still applies). 🔲
- A boss fight (e.g. Diababa) and a miniboss fight (e.g. Ook): third person for the whole fight. 🔲
- A fight on horseback (Hyrule Field Bulblins) and as the wolf (Twilight): third person. 🔲
- A cutscene that starts mid-fight plays as before. Afterwards, the view matches the combat state. 🔲
- The HUD stays visible during the fade. 🔲
- Third Person in Combat is greyed out when Perspective is Third Person. With it off, First Person is unchanged. 🔲
- Third Person Movement is greyed out in First Person with Third Person in Combat off, and available in the other two cases. 🔲
- Third Person Movement set to Headset: third person moves relative to the headset, as now. 🔲
- Third Person (Movement: Camera), camera behind Link: push forward and Link runs away from you. Turn your head 90° to the right while still holding forward: Link keeps running straight. Turn the camera (right stick): Link's direction turns with it. 🔲
- First person: forward still follows the headset. 🔲
- Run forward in first person into a fight, holding the stick: Link keeps his heading through the switch. Release the stick and push forward again: he moves relative to the third-person camera. 🔲
- Performance: no measurable FPS change, checked with `tools/metrics.sh` in a fight. 🔲

## Implementation (as built)
- `updateCombatCamera()` (`vr_main.cpp`) runs once per frame, before anything reads `isFirstPerson()`. It checks the main and sub BGM IDs against `kCombatBgmIds`, runs the delays (`CombatDelay`) and the fade (`ViewFade`), and publishes the result as `vr_link::g_combatThirdPerson`. Delays and fade count real frame time.
- `vr_link_visibility.hpp`: `isCombatThirdPersonView()` (switched in, no event running, not in a game first-person mode, not in clawshot flight) makes `isFirstPerson()` and `isWolfFirstPersonView()` return false. `isThirdPersonMode()` (Third Person choice or combat view) replaces the bare `vrThirdPerson` checks for Link's body visibility, Turn With Game Camera, third-person Z-target tracking and the first-person Z-target lock. `isGameFirstPersonProc()` lists the *_SUBJECT aim procs and *_SUBJECTIVITY look procs.
- Fade: `vr_render::drawViewFade()` draws one black, alpha-blended quad in view space after the 3D scene and the cutscene frame, before the HUD billboard (`drawCutsceneFrameIfActive()`).
- Movement: `d_a_alink.cpp` adds the stick angle to `dusk::vr::getMoveBasisAngleS()` instead of `getHeadMoveAngleS()`. Camera basis = `computeHeadWorldForward()` with the head orientation replaced by identity (smooth-turn yaw only). The hold rule is `MoveBasisHold`; "stick held" means stick value > 0.05.
- Every change of the main/sub BGM IDs or the combat state is logged (`combat camera: ...` in the game log), for checking which tracks count as combat.
- Pure logic (`CombatDelay`, `ViewFade`, `MoveBasisHold`, `angleDiffS`) is in `vr_math.hpp`, with host tests in `tests/vr/vr_math_tests.cpp` (suite "combat camera (spec 11)").
- `tools/scenarios/combat-camera.txt`: warps to Faron Woods (Ordon has no battle music), spawns a Bulblin next to Link, screenshots before, during and after the fight. Needs `game.vrThirdPersonInCombat` on.
- The movement fix also changes the plain Third Person choice, so it could go upstream on its own. It shares files with the combat switch, so a `pr/*` branch would need to take it apart by hand.
