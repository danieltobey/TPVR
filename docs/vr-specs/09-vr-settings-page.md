# 09 VR settings page

**Status:** 🟡 Built. Reworked 2026-10-15 to merge upstream's new settings (Giant Screen, Full Body, HUD on the wrist, Shadows, Sharpening, High Clocks) into this layout.

## Required behaviour
1. Sections, in order: View · Body · Movement & Turning · Cutscenes · HUD · Lighting & Shadows · Combat · Graphics · PC Display (PC only).
2. Every description is at most two sentences and states the default ("On by default." / "Off by default." / "Default: 90 cm.").
3. Multiple-choice settings name the default first, then one short line per option.
4. Settings that depend on another are greyed out when they don't apply:
   - in third person or Giant Screen: Camera Follows Animations, Smooth Movement, the Body section, HUD on Left Wrist;
   - outside third person: Turn With Game Camera;
   - in Giant Screen: Positional Tracking; outside Giant Screen: Giant Screen 3D and its depth (depth also needs 3D on);
   - Show Roll Animations and Arm Stretch Limit when Full Body is off;
   - TV Size/Distance/Height outside TV mode; Follow Cutscene Camera Turns without Face Cutscene Camera; the snap/smooth turn values for the other turning style; Shadows without Single-Pass Stereo.
5. "(Experimental)" goes at the end of a name.
6. Always-on fixes (object lighting, sun-glare dimming off, sky, audio) have no setting.
7. **View Mode** is one choice over three: First Person · Third Person · Giant Screen. Giant Screen keeps the first/third-person choice underneath, so leaving it returns there.
8. Settings that only exist on the standalone headset (Sharpening, High Clocks) are hidden on PC; PC-only ones (PC Display) are hidden on the headset.
9. Names follow what the player sees, not the config key. Upstream's settings are shown as:

   | Upstream name | Shown as |
   |---|---|
   | Giant Screen Mode | View Mode → Giant Screen |
   | Stereoscopic 3D Screen / 3D Screen Depth | Giant Screen 3D / Giant Screen 3D Depth |
   | Z-Target Camera Focus | Face Z-Target |
   | HUD on Left Arm | HUD on Left Wrist |
   | Shadows | Shadows (Experimental) |
   | Super Resolution (Sharpening) | Sharpening |

## Page contents
| Section | Settings |
|---|---|
| View | View Mode · Turn With Game Camera · Camera Follows Animations · Positional Tracking · Giant Screen 3D · Giant Screen 3D Depth |
| Body | Full Body · Show Roll Animations · Arm Stretch Limit |
| Movement & Turning | Smooth Movement · Snap Turn · Smooth Turn Speed · Snap Turn Angle · Turn With Epona · Face Z-Target |
| Cutscenes | Cutscene View · TV Size · TV Distance · TV Height · Face Cutscene Camera · Follow Cutscene Camera Turns · First-Person Cutscenes (Experimental) |
| HUD | HUD Distance · HUD Size · HUD on Left Wrist |
| Lighting & Shadows | Lighting · Shadows (Experimental) |
| Combat | Swap Sword/Shield Hands · Physical Sword |
| Graphics | VR Render Resolution · Sharpening · Single-Pass Stereo · High Clocks |
| PC Display | VR Desktop Mirror · VR Brightness Compensation · VR Brightness Compensation (SteamVR) |

Not on the page (config file only): Attach Body Rotation to Headset (known movement lock-ups), Application SpaceWarp (parked), display refresh rate (`game.vrDisplayRefreshRate`, 72 Hz; the performance target is 72 Hz, spec 13), the retired Show Link's Body. Upstream removed Body Follows Headset on 2026-10-14 (Full Body now always follows the headset).

## Verification
Walk the page in the headset: order, wording, defaults and greying match this spec, including switching View Mode through all three choices. 🟡
