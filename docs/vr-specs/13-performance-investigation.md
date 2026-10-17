# 13 Performance investigation (Quest 3, heavy scenes)

**Status:** Phase A 🚧 in progress (approved by Daniel 2026-10-15; his answers to the open questions are under *Decisions*). Phases B and C not started.

This spec is a **measurement project**, not a fix. Its product is (a) a repeatable benchmark that any later performance change is judged by, and (b) a findings report that ranks what costs frame time in busy areas. Each fix chosen from that report gets its own spec, built and verified against the same benchmark.

## Problem
Busy areas drop below the display rate (72 Hz), and the compositor then repeats or reprojects frames, which shows as judder. Daniel notices it most in non-combat scenes and depending on where he looks in a scene. We don't know *why* in any given scene, and the numbers we have disagree:

| Source | Scene | Result |
|---|---|---|
| Spec 03 A/B (2026-09-29) | central Ordon | ~65 FPS, GPU-bound |
| Spec 10 metrics walk (2026-10-01) | Ordon walk | 72.4 FPS, GPU 66%, CPU 38% |
| vr-mod-notes (2026-09-20, after single-pass) | "heaviest spot" | 50–52 FPS; GPU 12.4 ms cool, ~14 ms hot |

Three things make guessing unreliable:
- **Thermals.** The Quest 3 GPU throttles within minutes (about 640 → 545 MHz). Earlier A/Bs mixed cool and hot runs. This spec doesn't chase that: it always treats the headset as warm (see *Decisions*).
- **Fill-bound or geometry-bound is unknown.** Rendering at 90% "didn't help much" (2026-09-20). That hints the GPU's time goes into draw calls and vertices rather than pixels, but the planned 50% test was never run. This decides whether resolution levers (spec 12's dynamic resolution) can help at all.
- **The workload changed.** Upstream's 2026-10 merge added Full Body with arm IK (on by default), cloud shadows back in VR, optional object shadows (an extra scene traversal plus shadow maps), compositor sharpening (on), and high CPU/GPU clocks (on). None of these has been measured.

## What's already known and done (don't redo)
From `vr-mod-notes` (2026-09-16 … 09-20) and specs 08/10:
- Single-pass stereo: the scene is recorded once and replayed per eye; it renders directly into the swapchain image; depth is discarded when nothing reads it. Mid-eye screen captures are gone except the underwater blur. The minimap renders every other sim tick at 1.5× (it was ~4 ms of GPU).
- **The runtime overlaps CPU and GPU.** 4 ms of artificial main-thread time was fully absorbed, so the frame rate is set by whether the GPU's frame fits in one display period (13.9 ms at 72 Hz), unless the CPU side goes far over.
- Fixed foveated rendering is **not possible** on this renderer (Dawn has no fragment-density-map support). This answers spec 12's open question 4.
- Tools that work today: the runtime's per-second `VrApi` logcat line (FPS, stale frames, app GPU ms, CPU/GPU clocks, levels, temperature); Aurora's `[gpuprof]` per-pass GPU times (Android builds); `[dusk::vr::perf]` main-thread phase timing (only logged on dips over 22 ms); Tracy zones (`./build.sh tracy`); OVR Metrics CSVs (`tools/metrics.sh`); scenarios (`tools/scenario.sh`) with `warp`, `head lock`, scripted input, `wait` and `metrics start|stop`.

## Required behaviour

### Phase A: benchmark harness (tooling, dev builds only)
1. **Benchmark scenes.** Non-combat only. One scenario per scene in `tools/scenarios/bench/`: Ordon Village (`F_SP103`), South Faron Woods (`F_SP108`), Hyrule Field (`F_SP121`), Castle Town (`F_SP116`, the crowd scene: daytime, its streets full of townsfolk; **deferred**, see Decision 7), Kakariko Village (`F_SP109`), Lake Hylia (`F_SP115`), plus one light reference scene (an interior) to calibrate against. Each follows one fixed recipe:
   - warp to a fixed spot at a fixed time of day, then wait until the scene is fully populated (item 1a);
   - **viewpoint sweep:** with the head locked level, hold each of 8 fixed directions (every 45°) for 5 s, because cost depends on where you look. The instant 45° turn costs one missed frame, so each hold is measured from 0.5 s after the turn;
   - then a scripted ~20 s path of walking and turning.
1a. **The scene must be the real scene, not just the map.** NPCs, animals and objects appear according to story progress, time of day and room loading. So:
   - the benchmark plays from a **copy of Daniel's current save** placed in test slot 3 (slot 1 is only read, never written; back up first, rule 6), so the world is in his playthrough's state. The copy is frozen: it changes only when it's refreshed on purpose (`tools/bench.sh copy-save`), so the benchmark doesn't drift as he plays on. Console commands (devtools only): `save copy 1 3` copies the quest log in the game's own way (the file-select Copy: memory card read, slot copy with checksum, card write) and refuses any other target than slot 3; `save load 3 human` loads slot 3 into the game, as choosing it on the file select does, before the warp, with Link in human form (Decision 6; `wolf` or nothing keeps other options). `bench.sh copy-save` backs up the card file first and checks afterwards that only slot 3's bytes changed;
   - after the warp, the scenario waits until the loaded actor count stops rising (`wait settled`: unchanged for 3 s, at most 30 s), then checks it against that scene's expected minimum (`expect actors <n>`, recorded on the first run as 90% of the count it settled at and kept in the scenario). Too few actors marks the run **invalid** instead of measuring an empty map;
   - each scene's first run also takes a screenshot per direction, so Daniel can confirm the scene looks like his play (crowds present).
1b. **Segments.** The scenario marks each segment with `perf mark <name>` (`view0` … `view7`, `path`, `end`); between holds, unmeasured `gap<n>` segments hold the turn, its settle and any screenshot (a capture freezes the game ~3 s). A mark logs a timestamped line and closes the current perf log and `[gpuprof]` windows, so no summary line spans two segments. The summary assigns every source's lines to segments by their logcat time (the runtime's and OVR Metrics' once-a-second samples only when the whole second lies inside the segment). Known blur: GPU timings are read back a few frames late, so about 3 frames of the previous segment land in a segment's first `[gpuprof]` window; the summary ignores the `gap<n>` segments.
1c. **Save guard.** The guard from spec 11's branch (devtools builds stop auto-saving and point the file number at slot 3 once a remote command runs) is carried into this branch: `dev` lacked it.
1d. **Synthetic crowd scene (`crowd`, added 2026-10-16 at Daniel's request).** Because Castle Town is deferred (Decision 7), a scripted crowd stands in for crowd cost: Ordon's open village centre in daylight, plus a fixed set of passive townsfolk (Castle Town's townsfolk types, if they load outside the town; otherwise the closest that do) spawned by the devtools command `crowd <n>` at fixed positions in rings around Link (rings 120 units apart from 250, ~120 units between people, looks cycling through all 30, facing Link, standing still all day). It runs at four sizes, **0, 25, 50 and 100** extra characters (`crowd0`, `crowd25`, `crowd50`, `crowd100`), with the same sweep as the other scenes and, for the path, a slow look-around in place (walking would bump into the crowd differently at each size), so the summary gives the cost per extra character on the GPU and the main thread. Candidate types are test-spawned first and kept only if they appear and stay put. It complements, and does not replace, the real Castle Town.
2. **One command per run.** `tools/bench.sh <scene…|all> [--label <text>] [--repeat N]` runs the scenes back to back and records every source at once for each: the `VrApi` lines, `[gpuprof]`, the perf log (item 5), and the OVR Metrics CSV. It writes `logs/bench/<date>-<commit>-<label>/` with the raw logs, `summary.csv` and `summary.md`.
3. **Summary per scene and segment** (each viewpoint, and the path), with the worst viewpoint called out:
   - frame rate: average, 1% low, share of frames at the display rate, stale frames;
   - GPU frame time (average, 95th percentile) and the top passes by GPU time with their draw counts;
   - main-thread time split into sim tick / scene traversal and recording / waiting, plus render-worker time;
   - loaded actor count; CPU/GPU clocks and levels; **system temperature at the end of each run**.
4. **Conditions.** Always on USB power. The headset is assumed warm, however long it rested: no cool-down waits, no warm-up, no cool vs hot modes or corrections. Temperature and clocks are recorded (item 3), not controlled.
5. **Perf log on demand.** In devtools builds, the console command `perf log on|off` turns on a 1-per-second summary line (frame phases, render-worker time, draws, passes, actors drawn/culled, live particles), instead of only logging dips. The leftover 2026-09-20 diagnostics (`[passdump]`, dip lines, the `VR swapchain handoff` zone) move behind this same switch. Release builds log and listen for nothing new.
   - The line also counts how many display periods each frame stayed on screen (from the runtime's predicted display times: 1 = on time, 2+ = missed), which gives the share of frames at the display rate and the 1% low.
   - While the switch is on, `[gpuprof]` also lists each pass's draw count (Aurora change, on our Aurora branch), and `perf mark` flushes it.
6. **Comparison.** `tools/bench.sh compare <runA> <runB>` prints per-scene deltas, with each scene's measured run-to-run noise beside them, so a change smaller than the noise reads as "no change".
7. **Repeatability gate.** Run the same scene 3 times back to back: average FPS agrees within ±2 and GPU frame time within ±5% for each viewpoint. Until that holds, nothing measured with the harness counts.

### Phase B: baseline and diagnosis
8. **Baseline** every scene on current `dev` with the shipped defaults (Full Body on, Sharpening on, High Clocks on, Shadows off, 100% resolution, 72 Hz).
9. **Classify each scene.** CPU-bound (main thread or render worker over the period while the GPU waits) or GPU-bound. For GPU-bound scenes, a **resolution sweep** (50 / 70 / 100 / 130%, by restarting with a changed VR Render Resolution) tells pixel-bound from geometry/draw-bound: GPU time that scales with pixel count is fill; time that stays flat is geometry, draws or fixed passes.
10. **Cost of each contributor (ablation).** In devtools builds, `perf off <part>` / `perf on <part>` switch one part off at a time; the benchmark measures the difference in GPU ms and CPU ms. Parts: minimap render, HUD capture, cloud shadows, grass and flowers, particles, Full Body (body draw + arm IK), object shadows (on vs off), sharpening, bloom / depth of field / other post passes, the actor cull distance, sky. These switches are measurement-only and never appear in the settings page.
11. **Clocks.** Measure High Clocks on vs off, and whether a lower CPU level lets the GPU hold a higher clock when warm (shared thermal budget; hypothesis from 2026-09-20). The target is 72 Hz; higher refresh rates are out of scope.
12. **GPU counters, only if needed.** If the pass timings can't explain a GPU cost (e.g. bandwidth or binning on the tiled GPU), capture with a GPU profiler that works from Linux (candidates: Perfetto with Adreno counters, Snapdragon Profiler, RenderDoc Meta fork; check support first). Decided when reached.

### Phase C: findings
13. **Findings report** (`docs/perf/findings-<date>.md`): for each scene, what limits it, which viewpoints are worst, and the top costs with numbers. Then a ranked list of candidate fixes toward 72 Hz, each with its expected gain (ms), visual cost, effort and risk.
14. The report states whether **resolution has a significant impact** on frame time (item 9). Dynamic resolution (spec 12) is not part of this research; only if resolution matters is it re-discussed with Daniel. The report also covers parked levers such as Application SpaceWarp.
15. Daniel picks which fixes to do; each becomes its own spec, and its verification is "before/after on the benchmark".

## Settings
None for players. Devtools-only console commands: `perf log on|off`, `perf mark <name>`, `perf off|on <part>` (phase B), `save copy 1 3`, `save load 3 [human|wolf]`, `crowd <n>`, `wait settled`, `expect actors <n>`.

## Verification
1. With the game started (Daniel puts the headset on to launch it), `tools/bench.sh all` runs unattended under `tools/headset.sh awake` and produces a complete summary for every scene; `normal` afterwards.
1a. Every scene's run is valid (actor count at or above its minimum), and Daniel confirms from the first run's screenshots that the scenes, including the Castle Town crowd, look like his play.
2. The repeatability gate (item 7) passes for every scene.
3. A release build has no Tracy port open, no perf logs, and no `perf` commands.
4. Consistency check: switching off a scene's top-ranked cost with `perf off` cuts the frame time by about the measured share.
5. Daniel has reviewed the findings report and chosen what to fix.

Saves stay safe: benchmarks warp from the title, and devtools builds stop auto-saving and point at slot 3 as soon as a remote command runs (rule 6).

## Decisions (Daniel, 2026-10-15)
1. **Scenes:** non-combat (drops show most with viewpoint, less in combat). Include crowds. Actors and NPCs must be loaded, not just the map. The six suggested scenes are fine.
2. **Target:** 72 Hz.
3. **Temperature:** always assume a warm headset, even after long rests; no separate cool handling (it would waste time across development's heating and cooling). Record system temperature at the end of each run.
4. **Dynamic resolution:** not considered in this research until resolution is confirmed to have a significant impact; then re-discussed.
5. **Power:** USB. A warm headset may magnify impacts, which helps the measurements. (2026-10-16: a wall charger counts the same; the point is not running on battery.)
6. **Form (2026-10-16):** benchmark as human Link, even when the save is in wolf form: most play is as Link, and it exercises Full Body and its arm IK. The form is set in the loaded save before the warp (Midna won't transform where people can see). Found the same day: until free transformation is unlocked (event bit 0x0D04), the game forces the wolf everywhere while any province is under twilight, as in Daniel's save (Eldin), so `save load 3 human` also sets that bit, in memory only. Twilit scenes (Kakariko, in this save) can't hold human Link and run as the wolf, as Daniel plays them now; each scene file says which form it uses. The first Ordon/Faron/Kakariko runs of 2026-10-16 were all wolf.
7. **Castle Town deferred (2026-10-16):** at Daniel's point in the story Castle Town is still under twilight (no townsfolk), so it's left out of `all` until he has cleared Lanayru's twilight; then `bench.sh copy-save` refreshes the copy and its spot is picked. Its scenario stays in `tools/scenarios/bench/`.
