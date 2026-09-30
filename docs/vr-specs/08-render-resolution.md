# 08 Render resolution

**Status:** 🟡 Built.

## Problem
VR Render Resolution was clamped to 50–100%, so supersampling wasn't possible, and the Video tab's Internal Resolution does not affect VR: the eye passes are created at exactly `g_eyeImageWidth × g_eyeImageHeight`, independent of the window framebuffer scale (`VISetFrameBufferScale`). Verified by reading the code, not measured.

## Required behaviour
1. VR Render Resolution accepts 50–200% of the headset's recommended per-eye size, applied at startup.
2. The result is capped so the double-wide swapchain stays within the runtime's maximum image size.
3. The description states that the Video tab's resolution doesn't affect VR.

## Settings
| Setting | Values | Default |
|---|---|---|
| VR Render Resolution | 50–200% (applies after restart) | 100% |

## Verification
- Startup log line `eye image size ... scale X -> WxH` shows the scaled size (e.g. 1.5 → 2520×2640). 🟡
- Text and edges look sharper above 100%; FPS impact noted. 🟡
