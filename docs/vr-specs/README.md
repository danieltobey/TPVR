# VR feature specs

Spec-driven development for the TPVR comfort, lighting, audio and presentation work.

## Process

1. **Spec first.** A new feature starts as a spec in this folder: the problem, numbered required behaviour, settings and defaults, and how it will be verified. It is reviewed and approved before any code is written.
2. **Build to the spec.** Changes to a feature's behaviour update its spec in the same commit.
3. **Verify against the spec.** Each spec's *Verification* steps are run in the headset (and with logging where the behaviour is measurable), then its status is updated.

## Status key

| Status | Meaning |
|---|---|
| ✅ Verified | Built, and every verification step passed in the headset |
| 🟡 Built | Implemented and compiling, not yet (fully) verified |
| 📝 Proposed | Spec written, awaiting approval; no code yet |

## Specs

| Spec | Status |
|---|---|
| [01 First-person camera](01-first-person-camera.md) | ✅ Verified (perspective switch 🟡) |
| [02 Movement & turning](02-movement-and-turning.md) | ✅ Verified (Turn With Epona re-check, horse deadzone 🟡) |
| [03 Lighting](03-lighting.md) | ✅ Verified (Follow Look / Original modes 🟡) |
| [04 Sky](04-sky.md) | ✅ Verified |
| [05 Audio](05-audio.md) | ✅ Verified |
| [06 Cutscenes](06-cutscenes.md) | ✅ Verified (size/distance settings 🟡) |
| [07 HUD & menus](07-hud-and-menus.md) | 🟡 Built |
| [08 Render resolution](08-render-resolution.md) | 🟡 Built |
| [09 VR settings page](09-vr-settings-page.md) | 🟡 Built |
| [10 Dev tooling](10-dev-tooling.md) | ✅ Verified (horse deadzone / fill-light follow re-checks deferred) |
| Anti-aliasing | 📝 To be specified |
| 12 Dynamic resolution scaling (on `feature/dynamic-resolution`) | 📝 Proposed, on hold until spec 13 shows resolution matters |
| Performance investigation | 📝 To be specified |

All behaviour here is VR-only; flatscreen play is unchanged. Upstream's Giant Screen view mode (2026-10) shows the flat game in the headset, and the immersive-VR fixes (lighting, sky, audio, movement) are off while it's selected.
