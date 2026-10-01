#pragma once

// Pure VR maths (spec 10 item 6, 2026-09-29): quaternion rotation, the horse
// steering deadzone, TV and HUD panel sizing, and the low-pass / smoothing
// helpers. Standard library only -- no game, OpenXR or Aurora headers -- so
// the host unit tests in tests/vr/ can include it directly. Callers read
// settings and game state themselves and pass plain numbers in; the
// OpenXR/game-type wrappers live next to the call sites.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace dusk::vr::math {

struct Vec3 {
    float x, y, z;
};

// Unit quaternion, OpenXR component order and convention (Hamilton product).
struct Quat {
    float x, y, z, w;
};

inline constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------------

// Rotates v by unit quaternion q: v' = v + 2w(q x v) + 2 q x (q x v).
inline Vec3 rotateByQuat(const Quat& q, const Vec3& v) {
    const float tx = 2.f * (q.y * v.z - q.z * v.y);
    const float ty = 2.f * (q.z * v.x - q.x * v.z);
    const float tz = 2.f * (q.x * v.y - q.y * v.x);
    return Vec3{
        v.x + q.w * tx + (q.y * tz - q.z * ty),
        v.y + q.w * ty + (q.z * tx - q.x * tz),
        v.z + q.w * tz + (q.x * ty - q.y * tx),
    };
}

// Rotates v by the inverse (conjugate) of unit quaternion q.
inline Vec3 rotateByQuatInverse(const Quat& q, const Vec3& v) {
    return rotateByQuat(Quat{-q.x, -q.y, -q.z, q.w}, v);
}

// Same rotation as rotateByQuat, written as the rotation matrix R(q) times v.
// Kept as its own form because the hand calibration constants in
// vr_link_visibility.hpp were tuned against it.
inline Vec3 rotateByQuatMatrix(const Quat& q, const Vec3& v) {
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return Vec3{
        (1.f - 2.f * (yy + zz)) * v.x + 2.f * (xy - wz) * v.y + 2.f * (xz + wy) * v.z,
        2.f * (xy + wz) * v.x + (1.f - 2.f * (xx + zz)) * v.y + 2.f * (yz - wx) * v.z,
        2.f * (xz - wy) * v.x + 2.f * (yz + wx) * v.y + (1.f - 2.f * (xx + yy)) * v.z,
    };
}

// Rotates v around the vertical (+Y) axis by yawRad (positive = counter-
// clockwise seen from above, i.e. -Z forward turns towards -X).
inline Vec3 rotateYaw(const Vec3& v, float yawRad) {
    const float s = std::sin(yawRad);
    const float c = std::cos(yawRad);
    return Vec3{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

// RotateY(yawRad) * q: applies the same world-space yaw as rotateYaw() to an
// orientation. RotateY(yawRad) = (0, sin(yawRad/2), 0, cos(yawRad/2)).
inline Quat rotateYawQuat(const Quat& q, float yawRad) {
    const float hs = std::sin(yawRad * 0.5f);
    const float hc = std::cos(yawRad * 0.5f);
    return Quat{
        hc * q.x + hs * q.z,
        hc * q.y + hs * q.w,
        hc * q.z - hs * q.x,
        hc * q.w - hs * q.y,
    };
}

// Scales v to unit length; leaves (near-)zero vectors unchanged.
inline Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0001f) {
        return Vec3{v.x / len, v.y / len, v.z / len};
    }
    return v;
}

// ---------------------------------------------------------------------------
// Smoothing
// ---------------------------------------------------------------------------

// One step of a first-order low-pass filter: moves `current` the fraction
// `alpha` of the way to `target`. Called once per tick/frame, alpha is the
// per-step blend (0.1 at 30 ticks/s is a ~0.3 s time constant).
inline float lowPass(float current, float target, float alpha) {
    return current + (target - current) * alpha;
}

// Low-pass step for a direction: blends each component, then renormalizes
// (lerping two unit vectors shrinks the result).
inline Vec3 dampDirection(const Vec3& smoothed, const Vec3& target, float alpha) {
    return normalize(Vec3{
        lowPass(smoothed.x, target.x, alpha),
        lowPass(smoothed.y, target.y, alpha),
        lowPass(smoothed.z, target.z, alpha),
    });
}

// Frame-rate independent blend factor for an exponential follow with time
// constant `timeConstantSec`, over a step of `dtSec`.
inline float expBlend(float dtSec, float timeConstantSec) {
    return 1.f - std::exp(-dtSec / timeConstantSec);
}

// Moves angle `currentRad` towards `targetRad` by `blend` (0..1) of the
// difference, the shortest way round; result wrapped to [-pi, pi].
inline float followAngle(float currentRad, float targetRad, float blend) {
    const float delta = std::remainder(targetRad - currentRad, 2.f * kPi);
    return std::remainder(currentRad + delta * blend, 2.f * kPi);
}

// ---------------------------------------------------------------------------
// Horse steering (spec 02)
// ---------------------------------------------------------------------------

// Steering deadzone around the horse's own facing: |relDeg| up to
// deadzoneDeg steers straight (0), beyond it the angle ramps up from zero so
// that 180 still maps to 180 (remapped, not a hard cut-off, so a turn never
// kicks in abruptly at the threshold). Sign is preserved.
inline float horseSteerRemapDeg(float relDeg, float deadzoneDeg) {
    const float mag = std::fabs(relDeg);
    if (mag <= deadzoneDeg) {
        return 0.f;
    }
    const float out = (mag - deadzoneDeg) * (180.f / (180.f - deadzoneDeg));
    return relDeg < 0.f ? -out : out;
}

// ---------------------------------------------------------------------------
// Panels: TV (spec 06) and HUD (spec 07)
// ---------------------------------------------------------------------------

// tan(half the vertical angle) a TV screen subtends: diagonal diagCm at
// distance distCm, with aspect (width / height) clamped to [0.5, 3] and the
// distance to at least 1 cm. Height = diagonal / sqrt(1 + aspect^2).
inline float tvScreenTanHalfFovy(float diagCm, float distCm, float aspect) {
    const float dist = std::max(distCm, 1.f);
    const float a = std::clamp(aspect, 0.5f, 3.f);
    const float heightCm = diagCm / std::sqrt(1.f + a * a);
    return (heightCm * 0.5f) / dist;
}

// Pitch (radians, positive = up) that moves the TV's centre by heightPercent
// (clamped to [-50, 50]) of the screen's own height, for a screen whose
// half-height subtends atan(screenTanHalfFovy). Independent of distance.
inline float tvHeightPitch(float heightPercent, float screenTanHalfFovy) {
    const float share = std::clamp(heightPercent, -50.f, 50.f) / 100.f;
    return std::atan(share * 2.f * screenTanHalfFovy);
}

// Projection zoom so a camera with vertical fov camFovyDeg (clamped to
// [5, 150]) exactly fills a screen whose half-height subtends
// atan(screenTanHalfFovy).
inline float tvZoom(float screenTanHalfFovy, float camFovyDeg) {
    const float camHalf = std::clamp(camFovyDeg, 5.f, 150.f) * 0.5f * (kPi / 180.f);
    return screenTanHalfFovy / std::tan(camHalf);
}

struct PanelSize {
    float width, height;
};

// Width and height of a flat panel from its diagonal and its aspect given as
// height / width (the HUD texture's 448/608).
inline PanelSize panelFromDiagonal(float diagonal, float heightOverWidth) {
    const float width = diagonal / std::sqrt(1.f + heightOverWidth * heightOverWidth);
    return PanelSize{width, width * heightOverWidth};
}

// ---------------------------------------------------------------------------
// Combat camera and third-person movement (spec 11)
// ---------------------------------------------------------------------------

inline constexpr float kCombatEnterDelaySec = 0.5f;
inline constexpr float kCombatExitDelaySec = 2.0f;
inline constexpr float kViewFadeSec = 0.15f;
// How far (s16 binary angle) the stick may move from where it was at a view
// switch before the held movement basis is let go: 45 degrees.
inline constexpr int kMoveBasisHoldReleaseS = 0x2000;

// Combat as the camera sees it: the raw signal has to hold for the enter
// delay before it counts, and has to be gone for the exit delay before it
// stops counting. A blip back to the current state restarts the timer.
struct CombatDelay {
    bool active = false;
    float timerSec = 0.f;

    bool update(bool combatNow, float dtSec) {
        if (combatNow == active) {
            timerSec = 0.f;
            return active;
        }
        timerSec += dtSec;
        if (timerSec >= (active ? kCombatExitDelaySec : kCombatEnterDelaySec)) {
            active = combatNow;
            timerSec = 0.f;
        }
        return active;
    }
};

// Fade to black around a view switch. `shown` is the view the camera uses;
// it only changes to `wanted` on the frame the fade is fully black, then the
// fade clears again. If `wanted` flips back during the fade-out, the fade
// just clears without switching. With allowFade false the view switches at
// once and any fade is dropped.
struct ViewFade {
    bool shown = false;
    float alpha = 0.f;  // 0 = clear, 1 = black

    // Returns true on the frame the view switched.
    bool update(bool wanted, bool allowFade, float dtSec) {
        if (!allowFade) {
            const bool switched = shown != wanted;
            shown = wanted;
            alpha = 0.f;
            return switched;
        }
        const float step = dtSec / kViewFadeSec;
        if (wanted != shown) {
            alpha = std::min(1.f, alpha + step);
            if (alpha >= 1.f) {
                shown = wanted;
                return true;
            }
        } else {
            alpha = std::max(0.f, alpha - step);
        }
        return false;
    }
};

// Signed difference a - b of two s16 binary angles, in [-32768, 32767].
inline int angleDiffS(int16_t a, int16_t b) {
    return static_cast<int16_t>(static_cast<uint16_t>(a) - static_cast<uint16_t>(b));
}

// Which yaw the movement stick is relative to: the headset's, or the
// third-person camera's. When that choice changes while the stick is held,
// the last basis is kept (frozen) so Link keeps his heading, until the stick
// is released or turns more than kMoveBasisHoldReleaseS from where it was.
struct MoveBasisHold {
    bool valid = false;
    bool lastWantCamera = false;
    bool holding = false;
    int16_t lastBasis = 0;
    int16_t heldBasis = 0;
    int16_t heldStick = 0;

    int16_t update(bool wantCamera, int16_t headBasis, int16_t cameraBasis, bool stickHeld,
                   int16_t stickAngle) {
        if (valid && wantCamera != lastWantCamera && stickHeld && !holding) {
            holding = true;
            heldBasis = lastBasis;
            heldStick = stickAngle;
        }
        valid = true;
        lastWantCamera = wantCamera;
        if (holding &&
            (!stickHeld || std::abs(angleDiffS(stickAngle, heldStick)) > kMoveBasisHoldReleaseS)) {
            holding = false;
        }
        lastBasis = holding ? heldBasis : (wantCamera ? cameraBasis : headBasis);
        return lastBasis;
    }
};

}  // namespace dusk::vr::math
