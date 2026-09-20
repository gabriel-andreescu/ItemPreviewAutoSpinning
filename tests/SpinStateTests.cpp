#include "Settings.h"
#include "SpinState.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

TEST_CASE("Automatic rotation follows elapsed time and signed speed", "[spin]") {
    SpinState state;
    SpinSettings settings;
    float total = 0.0F;
    for (int frame = 0; frame < 60; ++frame) {
        total += state.Update(settings, false, 1.0F / 60.0F).x;
    }
    CHECK(total == Catch::Approx(0.3F));
    settings.rotationSpeed = -2.0F;
    CHECK(state.Update(settings, false, 0.02F).x == Catch::Approx(-0.04F));
    settings.rotationSpeed = 0.0F;
    CHECK(state.Update(settings, false, 0.02F).x == 0.0F);
}

TEST_CASE("Frame stalls are capped and nonpositive elapsed time does not rotate", "[spin]") {
    SpinState state;
    const SpinSettings settings;
    CHECK(state.Update(settings, false, 10.0F).x == Catch::Approx(0.015F));
    CHECK(state.Update(settings, false, 0.0F).x == 0.0F);
    CHECK(state.Update(settings, false, -1.0F).x == 0.0F);
}

TEST_CASE("Dragging takes priority and automatic rotation resumes after the delay", "[spin]") {
    SpinState state;
    SpinSettings settings;
    settings.resumeDelay = 0.1F;
    CHECK(state.Update(settings, true, 0.05F).x == 0.0F);
    CHECK(state.Update(settings, false, 0.05F).x == 0.0F);
    CHECK(state.Update(settings, false, 0.05F).x == 0.0F);
    CHECK(state.Update(settings, false, 0.05F).x == Catch::Approx(0.015F));
}

TEST_CASE("A recent flick continues on both axes and decays with elapsed time", "[spin]") {
    SpinState state;
    SpinSettings settings;
    settings.manualSpinDuration = 1.0F;
    (void)state.Update(settings, true, 0.01F);
    state.CaptureVelocity({2.0F, -1.0F});
    const auto first = state.Update(settings, false, 0.05F);
    CHECK(first.x == Catch::Approx(0.1F));
    CHECK(first.y == Catch::Approx(-0.05F));
    for (int frame = 1; frame < 20; ++frame) {
        (void)state.Update(settings, false, 0.05F);
    }
    const auto afterOneSecond = state.Update(settings, false, 0.05F);
    CHECK(afterOneSecond.x == Catch::Approx(first.x * 0.05F));
    CHECK(afterOneSecond.y == Catch::Approx(first.y * 0.05F));
}

TEST_CASE("Slow movement and stale flicks do not start release spin", "[spin]") {
    SpinState state;
    const SpinSettings settings;
    (void)state.Update(settings, true, 0.01F);
    state.CaptureVelocity({2.0F, 0.0F});
    SECTION("Slow repositioning clears the flick") {
        state.CaptureVelocity({0.1F, 0.1F});
    }
    SECTION("Holding still expires the flick") {
        for (int frame = 0; frame < 3; ++frame) {
            (void)state.Update(settings, true, 0.05F);
        }
    }
    CHECK(state.Update(settings, false, 0.01F).x == 0.0F);
}

TEST_CASE("Release spin respects its enable switch, strength, and speed limit", "[spin]") {
    SpinState state;
    SpinSettings settings;
    (void)state.Update(settings, true, 0.01F);
    state.CaptureVelocity({100.0F, 100.0F});
    SECTION("Disabled") {
        settings.manualSpinAfterDrag = false;
    }
    SECTION("Zero strength") {
        settings.manualSpinStrength = 0.0F;
    }
    SECTION("Limited speed") {
        const auto delta = state.Update(settings, false, 0.01F);
        CHECK(std::hypot(delta.x, delta.y) == Catch::Approx(0.08F));
        return;
    }
    CHECK(state.Update(settings, false, 0.01F).x == 0.0F);
}

TEST_CASE("Reset clears drag momentum and the pending delay", "[spin]") {
    SpinState state;
    const SpinSettings settings;
    (void)state.Update(settings, true, 0.01F);
    state.CaptureVelocity({2.0F, 1.0F});
    state.Reset();
    const auto delta = state.Update(settings, false, 0.01F);
    CHECK(delta.x == Catch::Approx(0.003F));
    CHECK(delta.y == 0.0F);
}
