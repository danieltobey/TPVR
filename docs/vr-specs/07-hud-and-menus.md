# 07 HUD & menus

**Status:** ✅ Verified: HUD at ~90 cm. 🟡 Built: HUD Size/Distance settings in cm, menu render-size cap.

## Problem
- At 1.7 m, the HUD/text-box billboard was drawn on top of objects closer than it, yet appeared behind them in stereo — a depth conflict causing eye strain.
- The Dusklight menu (RmlUi) renders at the Quest's window-surface size (~4128×2162) but is shown only ~900 px wide per eye, a ~4.5× minification without mipmaps: small text aliased and shimmered. The start screen's large text survived; the settings page didn't.

## Required behaviour
1. The HUD billboard's distance and physical size (diagonal) are configurable; defaults keep the original angular size.
2. While a VR session exists, RmlUi renders its canvas at most 1200 px wide, with its density ratio scaled by the same factor so the layout is identical (`aurora::rmlui::set_render_max_width()`). No cap outside VR.
3. Menu input (mouse/touch) mapping remains correct with the capped canvas.

## Settings
| Setting | Values | Default |
|---|---|---|
| HUD Distance | 50–300 cm | 90 cm |
| HUD Size (diagonal) | 20–400 cm | 93 cm |

## Verification
- Text boxes appear in front of nearby NPCs/walls with no strain. ✅
- HUD sliders change distance and size as a physical panel would. 🟡
- Settings text in the Dusklight overlay is sharp and stable, like the start screen. 🟡

## Open issues
- Start screen (screenshot 2026-09-29, after requirement 2): "✓ Disc ready" overlaps "QUIT", and the version text runs past the panel's right edge. Out of scope: this is an existing upstream issue, not caused by the canvas cap (confirmed by the tester, 2026-09-29).

## Notes
Requirement 2 changes `extern/aurora`; the PR needs an Aurora commit and a submodule bump.
