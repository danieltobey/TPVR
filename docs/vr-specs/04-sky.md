# 04 Sky

**Status:** ✅ Verified.

## Problem
The sky dome (`d_a_vrbox`, `d_a_vrbox2`), cloud layer and sun sprite are placed around the current camera eye, which in VR is the chase camera a few hundred units from the headset. The sun and clouds swung as that camera orbited Link (~3.5° for the sun at 8000 units). The cloud texture scroll was the wind crossed with the camera's facing, so clouds sped up, stopped or reversed as Link turned.

## Required behaviour
1. In VR, the sky dome, cloud layer and sun sprite are centred on and aimed from the headset eye.
2. In VR, cloud drift uses a fixed direction perpendicular to the wind: steady drift at the flatscreen cross-wind speed.

## Settings
None (always on in VR).

## Verification
On a clear day, walk and turn: sun and clouds stay fixed in the sky and drift steadily. ✅
