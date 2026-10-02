# 12 Dynamic resolution scaling

**Status:** 📝 Proposed (draft 2026-10-02, awaiting approval; open questions at the end).

## Problem
Render resolution is fixed for the whole session (spec 08: VR Render Resolution, read once at startup). Busy areas run below the headset's 72 Hz: central Ordon Village was measured at ~65 FPS, GPU-bound (CHANGES.md, *Performance*). Below 72 FPS the compositor repeats or reprojects frames, which shows as judder. Quiet areas have GPU headroom that goes unused. A resolution that drops a little in heavy scenes and comes back up in light ones would hold 72 FPS without lowering image quality everywhere.

## How it works (from reading the code, 2026-10-02)
- On the Quest the scene is drawn once per frame into one double-wide image (left eye | right eye) that is the XR swapchain's shared image (single-pass stereo, direct render). The swapchain is created once at startup at the VR Render Resolution size.
- OpenXR lets each frame tell the compositor which part of the swapchain image holds each eye (`imageRect`). The compositor stretches that part to the display. So the swapchain can stay at its full size while a frame draws into a smaller top-left area and reports that area: no swapchain rebuild, no restart, no hitch.
- The game sets its viewport in GameCube units (608×448) and Aurora scales that to the pass size. Telling Aurora the pass is smaller than the texture makes the whole scene, including the game's own in-frame copies (bloom, depth of field, minimap), land in the smaller area.
- Aurora already measures GPU time per frame on the Quest (timestamp queries; the `[gpuprof]` log lines). That is the signal the controller reads.

## Required behaviour
1. **Live resolution.** With Dynamic Resolution on, each frame renders both eyes at a per-axis scale between *Minimum* and *Maximum* of the headset's recommended size, and the submitted layer reports that size. Changing the scale never recreates the swapchain or interrupts play.
2. **Ceiling.** The swapchain is created at the Maximum scale (today's VR Render Resolution, 50–200%). Dynamic resolution never goes above it.
3. **Controller.** Reads the GPU frame time each frame (from Aurora's timestamps, a few frames late as the readings arrive):
   - Target: 90% of the display's frame time (12.5 ms at 72 Hz; derived from the runtime's display period, so 80/90/120 Hz work too).
   - **Drop fast:** when the recent GPU time (short average) is over target, lower the scale in proportion to the overshoot, within one or two steps.
   - **Rise slowly:** only after about one second continuously under 80% of the frame time, raise one step.
   - Steps of 5 percentage points; sizes rounded to multiples of 8 pixels. Changes at most every few frames, so the scale doesn't flicker between two values.
   - When the frame is CPU-bound (GPU time under target but frames still missed), it does not lower resolution: that wouldn't help and only blurs the image.
4. **Off = today's behaviour.** With Dynamic Resolution off, rendering is exactly as now: fixed at VR Render Resolution.
5. **What isn't affected.** HUD and menu billboards, the cutscene TV screen and the aim dot keep their own fixed-size textures; only the 3D scene's resolution changes. The menu-open and loading frames don't count toward the controller (their cost isn't representative).
6. **Fallbacks.** If GPU timestamps aren't available (some PC drivers), dynamic resolution stays at Maximum and logs once why. The two-pass stereo path (fallback setting) and Space Warp (parked) stay at fixed resolution in this version.
7. **Logging and tools (debug builds).** Every 2 s a log line: current scale, eye size, average and peak GPU ms, frames over budget. Console command `drs` prints the same; `drs <percent>` pins the scale (for A/B screenshots and measuring), `drs auto` returns to automatic.

## Settings (VR settings page, Graphics group)
| Setting | Values | Default |
|---|---|---|
| Dynamic Resolution | On / Off (live) | Off until verified, then On (question 1) |
| Minimum Resolution | 50–100%, step 5 (live) | 70% (question 2) |
| VR Render Resolution | 50–200%, applies after restart; with Dynamic Resolution on it is the *maximum* | 100% (unchanged) |

Descriptions follow spec 09's style (short, states the default).

## Code changes (planned)
- **Aurora** (`danieltobey/aurora`, next to the existing VR fork commit): `ExternalPassTarget` gains a render size smaller than the texture; the pass uses it for its viewport/scissor, the native-size scaling and in-pass copy bounds, while the attachments (color and the pooled depth) stay full size. One small accessor exposes the latest measured GPU frame time.
- **VR code** (`vr_main.cpp`, `vr_stereo_render.hpp`): swapchain at Maximum; per-frame render size from the controller; `imageRect` offset/extent per eye use it (right eye starts at the scaled width); the shared-image hand-off copies only the used area.
- **Controller maths** in `vr_math.hpp` (pure functions), with host unit tests in `tests/vr/` (step down on overshoot, slow rise, hysteresis, clamping, 8-pixel rounding).
- **Settings**: two new config values and UI rows.

## Verification
1. **Baseline** (DRS off): `tools/scenarios/ordon-walk-metrics.txt` gives FPS, GPU %, frame time; Tracy/`[gpuprof]` GPU ms in central Ordon. (Recorded measurements disagree: ~65 FPS GPU-bound vs 72 FPS / GPU 66% on the 2026-10-01 walk, so this is re-measured first.)
2. **Holds frame rate:** same walk with DRS on: 72 FPS (or the display rate) for ≥ 95% of frames; the log shows the scale dropping in Ordon.
3. **Recovers:** in a light area (e.g. the title or an interior), the scale returns to Maximum within a few seconds.
4. **Pinned scale looks right:** `drs 70` vs `drs 100` screenshots in Ordon: the whole scene fills each eye at both sizes; bloom, depth of field, shadows, water, minimap, HUD, menus, cutscene TV and aim dot all correct; no black or stretched edges, no eye misalignment.
5. **No visible pumping:** in the headset, walking between Ordon and its outskirts: resolution changes aren't noticeable as flicker or "breathing".
6. **Off is unchanged:** DRS off produces the same screenshots and FPS as before.
7. Unit tests pass; CI builds Android and Windows.

## Open questions for Daniel
1. **Default:** ship On once verified (recommended: it only lowers resolution when the frame rate would otherwise drop), or leave it Off as an opt-in?
2. **Minimum floor:** 70% recommended (below that the image gets visibly soft on Quest 3). Lower floor = more frame-rate safety in the worst scenes.
3. **Supersampling headroom:** with VR Render Resolution above 100% (say 130%), DRS would supersample in light areas and drop toward 100% or below in Ordon. Want that (recommended), or should DRS's ceiling be capped at 100% regardless?
4. **Related, separate spec:** the game doesn't use the Quest's fixed foveated rendering (`XR_FB_foveation`), which lowers resolution only at the lens edges where it's hard to see, often worth 10–20% GPU. Worth investigating under *Performance investigation*, before or alongside this.
