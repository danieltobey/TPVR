# 05 Audio

**Status:** ✅ Verified.

## Problem
`Z2Audience::setAudioCamera()` received the flatscreen chase camera, so positional sounds panned relative to a camera that isn't where the player's head is. Link's own sounds followed points on his hidden body (feet, mouth, hip), so e.g. footsteps panned sideways when looking sideways while running.

## Required behaviour
1. In VR, the audio listener is the headset: head-centre view, eye position and forward, substituted inside `setAudioCamera()` so every caller is covered.
2. In first-person VR, Link's three sound objects follow the listener position (updated every frame), so his footsteps, voice and body sounds are centred. They revert to his body in third person, cutscenes and wolf form.

## Settings
None (always on in VR).

## Verification
Turn the head near a steady sound source: it pans correctly and is in front when faced. Look sideways while running: footsteps stay centred. ✅
