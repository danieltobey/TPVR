# 11 Combat camera

**Status:** 📝 Proposed (2026-10-01), awaiting approval.

**Upstream:** candidate for a future upstream PR. Branch `feature/combat-camera` from `dev`, because it extends the Perspective choice on the regrouped settings page (specs 01 and 09), which upstream doesn't have yet.

## Problem
In first person, fights are hard to read: enemies flank you out of view and you can't see Link's sword swings or guard. Third person is better for fighting but worse for exploring. Players want first person for exploring and third person for fighting, without opening the settings menu each time.

## Required behaviour
1. A third Perspective choice, **First Person, Third in Combat**, plays in first person and switches to the third-person view (the same view as the Third Person choice, with Link's body shown) while Link is in combat.
2. **In combat** means any of these:
   - The game is playing its regular battle music (`Z2BGM_BATTLE_NORMAL`, `Z2BGM_BATTLE_TWILIGHT`), which the audio system starts when enemies near Link notice him (`Z2SoundObjMgr::searchEnemy()`).
   - The game is playing a boss, miniboss, face-off or horseback-battle track. These fights use their own music instead of the battle music. The full list of track IDs is kept in one table in the code, with a test that each ID exists.
3. **Enter delay:** combat must last 0.5 s before the view switches, so an enemy that notices Link for a moment doesn't trigger it.
4. **Exit delay:** the view returns to first person 2 s after combat ends. If combat starts again within those 2 s, the view stays in third person. This is on top of the battle music's own fade-out.
5. **Fade:** each switch, in either direction, fades the view to black, swaps the camera while the view is fully black, then fades back in. That takes about 0.15 s out and 0.15 s in. Only the 3D view fades: the HUD and text boxes stay visible.
6. **Aiming:** if Link aims an item while Z-targeting an enemy, he stays in third person, because the game aims at the target for him. If he aims without Z-targeting, the view switches to first person, as the original game does. This means the game's own first-person aim modes (bow, slingshot, boomerang, clawshot, Dominion Rod, Hawk, Ball and Chain, including the horse, canoe and underwater versions). That switch is **immediate, with no fade and no delay**, so aiming isn't slowed down. When Link stops aiming, the view returns to third person immediately if still in combat. Otherwise it stays in first person.
7. **Exceptions that keep their current behaviour:** cutscenes, dialogue and door/chest transitions follow the existing rules in `isFirstPerson()`. Combat only decides the view during normal gameplay. If a cutscene starts mid-fight, it uses the existing cutscene rules, and afterwards the view follows the current combat state without a fade, because the cutscene's own fade covers the change.
8. Riding Epona and wolf form are included: a fight on horseback or as the wolf switches to third person like any other fight.
9. The **First Person** and **Third Person** choices behave exactly as they do now.

## Settings
| Setting | Values | Default |
|---|---|---|
| Perspective | First Person / First Person, Third in Combat / Third Person | First Person |

Stored as a new setting, `game.vrThirdPersonInCombat` (bool), alongside the existing `game.vrThirdPerson`, so existing settings files keep working. Third Person takes priority if both are set. The Perspective description states what the new choice does. The delays and fade times are constants (0.5 s, 2 s, 0.15 s), not settings. They can be made into settings later if they need tuning.

Settings that are greyed out in third person (Camera Follows Animations, and any others) stay available in the new choice, because it is first person most of the time.

## Verification
- Walk up to a Bokoblin (Faron Woods, or Kakariko Gorge at night): the battle music starts, then the view fades to third person. Kill it: about 2 s after the music stops, the view fades back to first person. 🔲
- Walk past an enemy that notices Link for a moment and then loses him: the view doesn't switch, or doesn't flicker back and forth. 🔲
- In combat, Z-target an enemy and draw the bow: the view stays in third person and the arrow hits the target. 🔲
- In combat, draw the bow without Z-targeting: the view switches to first person right away, with no fade. Put the bow away: the view goes back to third person right away. 🔲
- Clawshot aim without Z-targeting in combat: first person, with no drift (the existing clawshot workaround still applies). 🔲
- A boss fight (e.g. Diababa) and a miniboss fight (e.g. Ook): third person for the whole fight. 🔲
- A fight on horseback (Hyrule Field Bulblins) and as the wolf (Twilight): third person. 🔲
- A cutscene that starts mid-fight plays as before. Afterwards, the view matches the combat state. 🔲
- The HUD stays visible during the fade. 🔲
- The First Person and Third Person choices are unchanged. 🔲
- Performance: no measurable FPS change, checked with `tools/metrics.sh` in a fight. 🔲

## Implementation notes (for review, not binding)
- The combat check is a small function (e.g. `isInCombat()`) that reads `Z2GetSeqMgr()->getSubBgmID()` / `getMainBgmID()` against the track table. The delays work like the game's other timers: a counter per state, ticked once per game tick, so they don't depend on frame rate.
- `isFirstPerson()` gets one new rule for the new choice: during normal gameplay, if in combat (after the delays) and not in a first-person aim mode, return false. This is the same way Third Person and wolf form already force third person. Showing Link's body follows the existing `vrThirdPerson` checks, extended to the new choice while it is in third person.
- There is no fade code in the VR layer yet. The fade will be a black, full-view overlay drawn over the 3D scene in both eyes, before the HUD layer. The camera swap happens on the frame where the overlay is fully opaque.
- The pure logic (enter/exit delays, the aim exception, fade timing) goes in `vr_math.hpp` or a small header next to it, with host unit tests in `src/tests/vr/`.
- A scenario script (`tools/scenarios/combat-camera.txt`) warps near an enemy and takes screenshots before, during and after combat, so the switch can be checked without a headset.
