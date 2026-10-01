# 10 Dev tooling

**Status:** Phase 1 🟡 Built: screenshot ✅, live mirror ✅, keep-awake awaiting the 30-minute check. Phase 2: remote console ✅, test driver ✅, unit tests 🟡 (host checks ✅; headset: TV, HUD, crawl/swim ✅, horse and fill light pending); CI ✅. Phase 3 🟡 Built (2026-10-01), headset checks pending; viewer and ndk-stack checked on the PC.

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

4. **Remote console.** `tools/cmd.sh "<command>" ["<command>" …]` runs Dusklight console commands in the running game (e.g. `warp F_SP103 0 0`, `tp x y z`, `help`) and prints their output; with no arguments it reads commands from stdin, one per line. Output is also written to the log (`[devtools]` prefix). Debug builds only.
   - Mechanism: request files `devtools/cmd/<id>.req` (ids start with a timestamp, so concurrent calls don't collide and run in order); the game checks about every 250 ms and answers in `<id>.out`.
   - Commands run at the start of a game tick, before game logic, never mid-draw. One console state persists across calls (`@found`, history), as in the in-game console.
5. **Test driver.** Console commands for scripted input, for repeatable scenarios. *(Refined and approved 2026-09-29.)*
   - `input stick <x> <y> <seconds>`: hold the left stick (x, y from −1 to 1, like the thumbstick; replaces the real stick while held).
   - `input button <name> <seconds>`: hold a game button (`a b x y z l r start up down left right`, GameCube names), added to any real presses.
   - `input turn <degrees>`: turn the view instantly, as smooth turn does (positive = right).
   - `input stop`: release all scripted input.
   - `menu close`: close any open menus. Menus block the game's controller input, and warping from the title screen leaves the title menu open.
   - Held input is timed in game ticks (30 per second), not wall-clock time, so the same script gives the same movement regardless of frame rate. Commands return immediately; use `wait` to let them play out.
   - `head lock|unlock`: while locked, the view uses a fixed head pose (level, facing play-space forward, at the recentre point) instead of the tracked one, so runs don't depend on where the headset is or who's wearing it. The compositor still reprojects to the real head in the headset; screenshots show the locked view. Controllers stay tracked (lay them down for repeatable shots).
   - **Scenarios run in the game.** `tools/scenario.sh <file>` sends the whole script as one request (the remote console mechanism, item 4); the game runs its lines in order:
     - `wait <seconds>`: pause the script, counted in game ticks.
     - `wait ready`: pause until a stage change has finished and Link is in the world (for use after `warp`).
     - `screenshot <name>`: capture the next frame (item 3), named `<scenario>-<name>`.
     - any console command.
     The script's output comes back when it ends; `scenario.sh` then pulls its screenshots into `logs/shots/` and prints their paths. Requests queue: a request sent while a script is running waits for it.
   - Scenario scripts live in `tools/scenarios/*.txt`; `#` starts a comment line.

6. **Host unit tests.** A small test program (doctest), built and run natively in the container (`tools/test.sh`, seconds, no headset), covering the pure maths: quaternion rotation helpers, horse deadzone remap, TV zoom and size, HUD sizing, stance-height filter, direction and angle smoothing. *(Approach approved 2026-09-29.)*
   - **Pure maths header.** The maths moves into `src/dusk/vr/vr_math.hpp`, which includes only the C++ standard library (its own small `Vec3`/`Quat` types; no game, OpenXR or Aurora headers). Its functions take plain numbers and return plain numbers.
   - **Call sites unchanged in behaviour.** The game code keeps reading settings and game state itself and calls these functions with the values; the formulas are moved, not changed.
   - **Standalone test build.** `tests/vr/` is its own CMake project (doctest header vendored in `extern/doctest/`), independent of the game and Android builds. Compiled with the latest stable clang in the container (clang 23 from apt.llvm.org).
7. **CI on the fork.** GitHub Actions enabled on `danieltobey/TPVR`, so every push builds Android and Windows. Confirms the Windows build compiles before a PR. *(Details approved and built 2026-10-01.)*
   - **Own workflow file.** `.github/workflows/fork-ci.yml`, which runs only on `danieltobey/TPVR`. Upstream's `build.yml` is left unchanged and switched off on the fork in its Actions settings, so upstream merges never conflict. (It fails on every desktop job upstream anyway: Linux and macOS have no VR support, and Windows never installs the OpenXR loader, so the VR code is skipped and linking fails.)
   - **Three jobs**, on every push to any branch of the fork and on demand; pushes that only change docs or `*.md` skip them:
     1. *Unit tests*: `tests/vr` built with the latest stable clang and run (Linux runner, ~1 min).
     2. *Android*: the arm64 debug APK as `build.sh` makes it (developer tooling on), uploaded as a downloadable artifact.
     3. *Windows*: MSVC x86_64 with the OpenXR loader from vcpkg, so the VR code is compiled and linked. Windows arm64, Linux and macOS are not built.
   - **Aurora fork.** The menu fix's Aurora commit exists only locally, so CI can't fetch it. Fork `encounter/aurora` to `danieltobey/aurora` and push the `tpvr-vr-menu-cap` branch there; CI points the submodule at that fork before checkout (`.gitmodules` unchanged). This is also the fork the follow-up upstream PR needs.
   - **Publishing.** Push the build branch (`fix/stable-fp-camera`, renamed `dev` on 2026-10-01) to the fork (public). From then on, pushing is how a CI run starts.
   - The Android APK is signed with the CI machine's own debug key, so it can't update an installed dev build without an uninstall (which wipes that build's saves); use it for checking, and keep installing with `install.sh`.
   - `fork-ci.yml` is fork-only and stays out of upstream PRs. The OpenXR line for Windows could be offered upstream separately.

## Phase 3: Performance and debugging

8. **Tracy profiling.** The game already contains the Tracy profiler (Aurora fetches Tracy 0.14.1); it is compiled out by default. *(Details approved 2026-10-01.)*
   - **Build option.** `./build.sh tracy` builds with Tracy on; plain `./build.sh` builds with it off. Switching recompiles most of the game (Tracy changes the code inside every profiled function), so expect one long build each way. On-demand mode (Aurora's default) stays on: nothing is recorded until the viewer connects.
   - **Viewer.** Tracy 0.14.1, the same version as the game (Tracy needs viewer and game versions to match; Homebrew only has 0.13.1). Built from source in the container with Tracy's own bundled libraries, into `toolchains/tracy/`, and run on the host: the container can't use the NVIDIA driver (same reason scrcpy runs on the host). `tools/tracy.sh` forwards the game's Tracy port over adb (`adb forward tcp:8086`) and opens the viewer connected to it, so it works over Wi-Fi or USB without knowing the headset's IP. If the pinned Tracy version changes, `tools/tracy.sh` rebuilds the viewer. *(Added in the build:)* `tools/tracy.sh capture <seconds>` records a trace to `logs/tracy/` with Tracy's command-line capture tool and prints count, mean and max time per VR zone, so a run can be measured without anyone watching the viewer.
   - **VR zones.** A few named zones in the VR frame code (waiting for the headset's frame, rendering each eye, submitting the frame), so VR's own cost shows up next to the game's existing zones. They compile to nothing when Tracy is off.
9. **OVR Metrics Tool.** Meta's metrics app (frame rate, CPU/GPU load and clocks, thermal level, dropped frames), installed once in the headset from the Meta Horizon Store (free). *(Details approved 2026-10-01.)*
   - `tools/metrics.sh start` turns on its CSV recording over adb; `tools/metrics.sh stop` turns it off, pulls the new CSV files into `logs/metrics/` and prints a short summary (average and worst-1% FPS, average CPU and GPU load) so one run can be compared with another. `tools/metrics.sh overlay on|off` shows or hides its in-headset graph.
   - Scenarios can bracket a measurement: the test driver gets a `metrics start|stop` line that `scenario.sh` acts on, so the same scripted walk gives comparable numbers. The game only marks the times (device clock); `scenario.sh` records around the whole script and the summary covers just that window. The CSV's `Time Stamp` is ms since the game started and its file name ends with the local time recording began, which is how the window is mapped (to about 1 s; samples are 1 per second).
10. **Crash symbolication.** `tools/crash.sh` reads the game's own crash report (borealis's crash handler writes every frame with its offset and build ID into the game's log file, read with `run-as`, which debug APKs allow), takes the latest one, and symbolises the `libmain.so` frames with the NDK's `llvm-symbolizer` against the unstripped `libmain.so` from the last build, giving function names (inlined calls included) with file:line. The symbolised stack, the game's report and the tail of Android's crash log (`adb logcat -b crash`, for the signal and registers) are saved to `logs/crashes/<log name>.txt`. Frames whose build ID doesn't match the local `libmain.so` (the installed APK is from a different build) are listed unsymbolised with a warning instead of misleading lines. *(Details approved 2026-10-01; source changed in the headset check: on the Quest, Android's crash log has only one unnamed frame, because its unwinder can't read `libmain.so` inside the APK, so `ndk-stack` can't use it.)*
    - A debug-only console command `crash` crashes the game on purpose (null pointer write), for checking this tool.
11. **Native debugger.** `tools/debug.sh` attaches the NDK's LLDB (in the container) to the running game: it copies the NDK's `lldb-server` into the app (debug APKs allow `run-as`), starts it, connects over adb and loads the symbols from the last build. With no arguments it opens an interactive LLDB session; extra arguments are passed to LLDB, so it can also run non-interactively (e.g. `tools/debug.sh -o "b daAlink_c::execute" -o c -o bt -o detach`). The recipe is also written out in the script's header. *(Details approved 2026-10-01.)*
    - Pausing the game at a breakpoint freezes its frames; the headset shows its loading dots until it continues.

## Verification
| # | Check |
|---|---|
| 1 | With `awake`, the headset stays connected for 30+ minutes off-head; `normal` restores sleep. |
| 2 | scrcpy shows the live view. ✅ (scrcpy 4.1) |
| 3 | `screenshot.sh` returns the side-by-side PNG of both eyes within ~2 s. ✅ |
| 4 | `cmd.sh "warp …"` warps the game; output appears in the log. ✅ (`warp F_SP103 0 0` → Ordon, `pos` confirms; 2026-09-29) |
| 5 | A scenario (warp to Ordon, `wait ready`, head lock, run forward 3 s, turn 90°, screenshot) run twice gives the same `pos` (within 1 unit) and screenshots that match by eye. Not pixel-identical: animals, NPCs, water and wind animate independently of input. ✅ (`tools/scenarios/ordon-walk.txt` twice: identical `pos`, 0.4% of pixels differ; 2026-09-29) |
| 6 | `test.sh` runs all tests in under a minute; a deliberately broken helper fails a test. ✅ (21 tests, ~2 s; a broken horse remap fails 2; 2026-09-29) In the headset, the moved maths behaves as before: horse steering deadzone, TV size in a cutscene, HUD size, crawl/swim eye height, HUD and fill-light smoothing. ✅ TV size, HUD size and lag, crawl/swim height (2026-10-01); horse deadzone and fill-light follow not yet checked (no horse / dark area). |
| 7 | A push to the fork produces green unit-test, Android and Windows jobs and a downloadable APK; a deliberately broken test turns the run red. ✅ (2026-10-01: first run green in 33 min (tests 1, Android 12, Windows 33, cold cache), APK artifact 35 MB; a broken test on a throwaway branch failed the Unit tests job in 1 min.) |
| 8 | With a `tracy` build, `tools/tracy.sh` shows live frames from the headset, including the VR zones; a normal build has no Tracy port open. ✅ (2026-10-01: normal build, nothing listening on 8086; `tracy` build, a 10 s capture had 726 frames with all VR zones, e.g. xrWaitFrame 6.7 ms, both eyes 2.7 ms, tick 10.2 ms mean at the title. The viewer window opened on the host; Daniel's look at the live view pending.) |
| 9 | `metrics.sh start` … `stop` around the Ordon walk scenario puts a CSV in `logs/metrics/` and prints the summary. ✅ (2026-10-01, `ordon-walk-metrics.txt`: 16 samples in the window, 72.4 FPS average, CPU 38%, GPU 66%.) Overlay on/off: pending a look in the headset. |
| 10 | `cmd.sh crash`, then `crash.sh`, gives a stack naming the `crash` command's function with its file:line. ✅ (2026-10-01: `crashOnPurpose()` at `vr_devtools_console.cpp:149`, then `step()`, `processRemoteCommands()`, `main01()`, `game_main()`; 8 frames.) |
| 11 | `debug.sh` stops at a breakpoint in `daAlink_c::execute()`, prints a backtrace with file:line, and the game carries on after detaching. ✅ (2026-10-01: stopped at `d_a_alink.cpp:18105`, 5-frame backtrace, game answered `pos` after detach. Android's Java runtime raises SIGSEGV on purpose, so `debug.sh` passes it through without stopping.) |

## Out of scope
Anti-aliasing, dynamic resolution and the performance fixes themselves (separate specs); GPU-level profilers (RenderDoc Meta fork, Snapdragon Profiler), pending a check of Linux support.

## Open questions
- ~~CI minutes on the fork are free for public repos; confirm the fork is public.~~ Public, Actions enabled (2026-10-01).
- Should phase 2's debug commands also be available in release builds for future bug reports? (Proposed: no.)
