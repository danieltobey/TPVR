// Host unit tests for src/dusk/vr/vr_math.hpp (spec 10 item 6).
// Build and run: tools/test.sh in ~/Documents/TPVR-dev.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "dusk/vr/vr_math.hpp"

#include <cmath>

using namespace dusk::vr::math;
using doctest::Approx;

namespace {

constexpr float kEps = 1e-5f;

float deg(float rad) { return rad * 180.f / kPi; }
float rad(float deg) { return deg * kPi / 180.f; }

void checkVec(const Vec3& a, const Vec3& b, float eps = kEps) {
    CHECK(a.x == Approx(b.x).epsilon(eps));
    CHECK(a.y == Approx(b.y).epsilon(eps));
    CHECK(a.z == Approx(b.z).epsilon(eps));
}

float length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// Rotation of yawRad about +Y as a quaternion.
Quat yawQuat(float yawRad) { return Quat{0.f, std::sin(yawRad * 0.5f), 0.f, std::cos(yawRad * 0.5f)}; }

// An arbitrary non-trivial unit quaternion (axis (1,2,3), 70 degrees).
Quat someQuat() {
    const float len = std::sqrt(14.f);
    const float s = std::sin(rad(35.f)), c = std::cos(rad(35.f));
    return Quat{s / len, 2.f * s / len, 3.f * s / len, c};
}

}  // namespace

TEST_SUITE("rotation") {
    TEST_CASE("identity quaternion leaves vectors unchanged") {
        const Vec3 v{1.f, -2.f, 3.f};
        checkVec(rotateByQuat(Quat{0, 0, 0, 1}, v), v);
        checkVec(rotateByQuatInverse(Quat{0, 0, 0, 1}, v), v);
        checkVec(rotateByQuatMatrix(Quat{0, 0, 0, 1}, v), v);
    }

    TEST_CASE("90 degrees about +Y turns forward (-Z) to the left (-X)") {
        checkVec(rotateByQuat(yawQuat(rad(90.f)), Vec3{0, 0, -1}), Vec3{-1, 0, 0});
        checkVec(rotateYaw(Vec3{0, 0, -1}, rad(90.f)), Vec3{-1, 0, 0});
    }

    TEST_CASE("inverse undoes the rotation") {
        const Quat q = someQuat();
        const Vec3 v{0.3f, -1.2f, 2.5f};
        checkVec(rotateByQuatInverse(q, rotateByQuat(q, v)), v);
        checkVec(rotateByQuat(q, rotateByQuatInverse(q, v)), v);
    }

    TEST_CASE("vector form and matrix form agree") {
        const Quat q = someQuat();
        const Vec3 v{0.3f, -1.2f, 2.5f};
        checkVec(rotateByQuatMatrix(q, v), rotateByQuat(q, v));
    }

    TEST_CASE("rotation preserves length") {
        const Vec3 v{0.3f, -1.2f, 2.5f};
        CHECK(length(rotateByQuat(someQuat(), v)) == Approx(length(v)).epsilon(kEps));
    }

    TEST_CASE("rotateYawQuat applies the same yaw as rotateYaw") {
        const Quat q = someQuat();
        const Vec3 v{0.3f, -1.2f, 2.5f};
        for (float yawDeg : {-170.f, -45.f, 0.f, 30.f, 90.f, 179.f}) {
            CAPTURE(yawDeg);
            const float yaw = rad(yawDeg);
            checkVec(rotateByQuat(rotateYawQuat(q, yaw), v), rotateYaw(rotateByQuat(q, v), yaw));
        }
    }

    TEST_CASE("normalize") {
        CHECK(length(normalize(Vec3{3.f, 4.f, 12.f})) == Approx(1.f).epsilon(kEps));
        const Vec3 tiny{0.f, 0.f, 0.00001f};
        checkVec(normalize(tiny), tiny);  // near-zero left alone, no divide by ~0
    }
}

TEST_SUITE("smoothing") {
    TEST_CASE("lowPass moves alpha of the way") {
        CHECK(lowPass(0.f, 10.f, 0.1f) == Approx(1.f));
        CHECK(lowPass(5.f, 5.f, 0.1f) == Approx(5.f));
        CHECK(lowPass(2.f, 10.f, 0.f) == Approx(2.f));
        CHECK(lowPass(2.f, 10.f, 1.f) == Approx(10.f));
    }

    TEST_CASE("stance-height filter: 0.1 per tick settles in ~0.3 s at 30 ticks/s") {
        // Step from 0 to 1: after one time constant (~9-10 ticks) about
        // 1 - 1/e of the way; after 1 s (30 ticks) within 5%; no overshoot.
        float h = 0.f;
        for (int tick = 1; tick <= 30; ++tick) {
            h = lowPass(h, 1.f, 0.1f);
            CHECK(h <= 1.f);
            if (tick == 10) {
                CHECK(h == Approx(1.f - std::exp(-1.f)).epsilon(0.05));
            }
        }
        CHECK(h > 0.95f);
    }

    TEST_CASE("dampDirection stays unit length and converges") {
        Vec3 d{0.f, 0.f, -1.f};
        const Vec3 target = normalize(Vec3{1.f, 0.2f, 0.f});
        for (int i = 0; i < 200; ++i) {
            d = dampDirection(d, target, 0.08f);
            CHECK(length(d) == Approx(1.f).epsilon(kEps));
        }
        checkVec(d, target, 1e-3f);
    }

    TEST_CASE("expBlend is frame-rate independent") {
        CHECK(expBlend(1.f, 1.f) == Approx(1.f - std::exp(-1.f)));
        CHECK(expBlend(0.f, 1.f) == Approx(0.f));
        // Two half steps = one full step.
        const float half = expBlend(0.5f, 1.f);
        const float full = expBlend(1.f, 1.f);
        CHECK(1.f - (1.f - half) * (1.f - half) == Approx(full).epsilon(kEps));
    }

    TEST_CASE("followAngle goes the shortest way round and wraps") {
        CHECK(deg(followAngle(rad(10.f), rad(30.f), 0.5f)) == Approx(20.f).epsilon(1e-4));
        // 170 -> -170 is 20 degrees through 180, not 340 back through 0.
        CHECK(deg(followAngle(rad(170.f), rad(-170.f), 0.5f)) == Approx(180.f).epsilon(1e-3));
        CHECK(deg(followAngle(rad(170.f), rad(-170.f), 1.f)) == Approx(-170.f).epsilon(1e-3));
        const float r = followAngle(rad(179.f), rad(-179.f), 0.9f);
        CHECK(r >= -kPi);
        CHECK(r <= kPi);
    }
}

TEST_SUITE("horse steering") {
    constexpr float kDeadzone = 20.f;  // d_a_horse.cpp kVrHorseSteerDeadzoneDeg

    TEST_CASE("inside the deadzone steers straight") {
        CHECK(horseSteerRemapDeg(0.f, kDeadzone) == 0.f);
        CHECK(horseSteerRemapDeg(10.f, kDeadzone) == 0.f);
        CHECK(horseSteerRemapDeg(-20.f, kDeadzone) == 0.f);
    }

    TEST_CASE("ramps from zero at the edge, no jump") {
        CHECK(horseSteerRemapDeg(20.1f, kDeadzone) == Approx(0.1125f).epsilon(1e-3));
        CHECK(horseSteerRemapDeg(20.1f, kDeadzone) < 0.2f);
    }

    TEST_CASE("full range still reaches 180 and keeps sign") {
        CHECK(horseSteerRemapDeg(180.f, kDeadzone) == Approx(180.f));
        CHECK(horseSteerRemapDeg(-180.f, kDeadzone) == Approx(-180.f));
        CHECK(horseSteerRemapDeg(100.f, kDeadzone) == Approx(90.f));
        CHECK(horseSteerRemapDeg(-100.f, kDeadzone) == Approx(-90.f));
    }

    TEST_CASE("monotonic") {
        float prev = horseSteerRemapDeg(-180.f, kDeadzone);
        for (float a = -179.f; a <= 180.f; a += 1.f) {
            const float out = horseSteerRemapDeg(a, kDeadzone);
            CHECK(out >= prev);
            prev = out;
        }
    }
}

TEST_SUITE("panels") {
    TEST_CASE("TV defaults: 760 cm diagonal at 500 cm, 16:9 is ~40.9 degrees tall") {
        const float t = tvScreenTanHalfFovy(760.f, 500.f, 16.f / 9.f);
        CHECK(2.f * deg(std::atan(t)) == Approx(40.9f).epsilon(0.005));
    }

    TEST_CASE("TV screen: twice as far looks half as tall") {
        const float near = tvScreenTanHalfFovy(760.f, 500.f, 16.f / 9.f);
        const float far = tvScreenTanHalfFovy(760.f, 1000.f, 16.f / 9.f);
        CHECK(far == Approx(near * 0.5f));
    }

    TEST_CASE("TV screen: aspect and distance are clamped") {
        CHECK(tvScreenTanHalfFovy(760.f, 500.f, 10.f) == Approx(tvScreenTanHalfFovy(760.f, 500.f, 3.f)));
        CHECK(tvScreenTanHalfFovy(760.f, 500.f, 0.1f) == Approx(tvScreenTanHalfFovy(760.f, 500.f, 0.5f)));
        CHECK(std::isfinite(tvScreenTanHalfFovy(760.f, 0.f, 16.f / 9.f)));
    }

    TEST_CASE("TV zoom is 1 when the camera fov matches the screen") {
        const float t = tvScreenTanHalfFovy(760.f, 500.f, 16.f / 9.f);
        const float screenFovyDeg = 2.f * deg(std::atan(t));
        CHECK(tvZoom(t, screenFovyDeg) == Approx(1.f).epsilon(1e-4));
        // A wider camera must be zoomed out (< 1) to fit, a narrower one in.
        CHECK(tvZoom(t, screenFovyDeg * 2.f) < 1.f);
        CHECK(tvZoom(t, screenFovyDeg * 0.5f) > 1.f);
        // fov clamped to [5, 150].
        CHECK(tvZoom(t, 1.f) == Approx(tvZoom(t, 5.f)));
        CHECK(tvZoom(t, 179.f) == Approx(tvZoom(t, 150.f)));
    }

    TEST_CASE("HUD default: 93 cm diagonal is ~75 cm wide") {
        const PanelSize p = panelFromDiagonal(93.f, 448.f / 608.f);
        CHECK(p.width == Approx(74.9f).epsilon(0.005));
        CHECK(p.height / p.width == Approx(448.f / 608.f));
        CHECK(std::sqrt(p.width * p.width + p.height * p.height) == Approx(93.f));
    }
}
