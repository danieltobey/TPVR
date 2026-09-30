# 10 Dev tooling

**Status:** Phase 1 🟡 Built: screenshot ✅, live mirror ✅, keep-awake awaiting the 30-minute check. Phases 2–3 📝 Approved, not started.

## Problem
Development depends on the tester relaying what they see, and on a headset connection that drops whenever the Quest sleeps (stale Wi-Fi adb, interrupted installs). Changes can't be checked without someone wearing the headset, the Windows build is never compiled, and there's no fast way to catch regressions in the maths or to profile performance.

## Scope
Tooling for the dev environment in `~/Documents/TPVR-dev` and debug-only code in TPVR. Nothing here changes gameplay or ships enabled in normal play.

## Phase 1: Headset access

1. **Keep awake.** `tools/headset.sh awake|normal` sends the Quest proximity-sensor override (`com.oculus.vrpowermanager.prox_close`) and restores it (`automation_disable`). While "awake", the headset doesn't sleep and apps keep running off-head. `install.sh` applies it automatically before installing.
2. **Live mirror.** scrcpy (latest, Homebrew on the host; the container can't use the host GPU driver) launched with `tools/mirror.sh`, using the Android SDK's adb, to show the headset's view in a window on the PC.
3. **In-game screenshot.** A debug command, triggered from the PC, makes the game write the next rendered frame of each eye as PNGs to its app storage; `tools/screenshot.sh [name]` triggers it, pulls the files into `logs/shots/` and prints their paths. It captures exactly what the renderer drew, independent of the system compositor.
   - Trigger mechanism: the game checks a trigger file in its app storage once per second (no Android intent plumbing needed).
   - Debug builds only.

## Phase 2: Automation and safety nets

4. **Remote console.** `tools/cmd.sh "<command>"` runs a Dusklight console command in the running game (e.g. `warp F_SP103 0 0`, `tp x y z`), via the same trigger-file mechanism, with output written to the log. Debug builds only.
5. **Test driver.** Console commands for scripted input, for repeatable scenarios:
   - `input stick <x> <y> <seconds>`: hold the left stick.
   - `input turn <degrees>`: rotate the view (smooth-turn yaw).
   - `input button <name> <seconds>`
   - `head lock|unlock`: freeze the head pose so runs don't depend on who's wearing it.
   Scenario scripts (`tools/scenarios/*.txt`) chain these with `screenshot` and `wait`.
6. **Host unit tests.** A small test target (doctest), built and run natively in the container (`tools/test.sh`, seconds, no headset), covering the pure maths: quaternion rotation helpers, horse deadzone remap, TV zoom and size, HUD sizing, stance-height filter, interpolation.
7. **CI on the fork.** GitHub Actions enabled on `danieltobey/TPVR`, so every push builds Android and Windows. Confirms the Windows build compiles before a PR.

## Phase 3: Performance and debugging

8. **Tracy profiling.** A build option to enable the already-integrated Tracy profiler; the Tracy viewer runs on the PC (container) and connects to the headset over Wi-Fi.
9. **OVR Metrics Tool** installed on the headset; `tools/metrics.sh` pulls its CSV captures into `logs/`.
10. **Crash symbolication.** `tools/crash.sh` pulls the latest crash and runs `ndk-stack` against the build's symbols, giving file:line stack traces.
11. **Native debugger.** A documented recipe for attaching LLDB (from the NDK) to the running debug APK.

## Verification
| # | Check |
|---|---|
| 1 | With `awake`, the headset stays connected for 30+ minutes off-head; `normal` restores sleep. |
| 2 | scrcpy shows the live view. ✅ (scrcpy 4.1) |
| 3 | `screenshot.sh` returns the side-by-side PNG of both eyes within ~2 s. ✅ |
| 4 | `cmd.sh "warp …"` warps the game; output appears in the log. |
| 5 | A scenario (warp to Ordon, run forward 3 s, screenshot) produces the same screenshots twice in a row. |
| 6 | `test.sh` runs all tests in under a minute; a deliberately broken helper fails a test. |
| 7 | A push to the fork produces green Android and Windows builds. |
| 8 | The Tracy viewer shows live frame zones from the headset. |
| 9 | Metrics CSV lands in `logs/`. |
| 10 | A deliberate test crash yields a symbolised stack. |
| 11 | LLDB stops at a breakpoint in `daAlink_c::execute()`. |

## Out of scope
Anti-aliasing, dynamic resolution and the performance fixes themselves (separate specs); GPU-level profilers (RenderDoc Meta fork, Snapdragon Profiler), pending a check of Linux support.

## Open questions
- CI minutes on the fork are free for public repos; confirm the fork is public.
- Should phase 2's debug commands also be available in release builds for future bug reports? (Proposed: no.)
