#include "BoundsMath.h"
#include <RE/N/NiBound.h>
#include <RE/N/NiPoint3.h>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace {
RE::NiBound Bound(float centerX, float radius) {
    return {.center = {centerX, 0.0F, 0.0F}, .radius = radius};
}
}

TEST_CASE("Normal preview bounds retain the engine pivot", "[bounds]") {
    const auto root = Bound(0.0F, 10.0F);
    const std::array samples {root};
    CHECK_FALSE(SelectRotationBound(root, samples).has_value());
    CHECK_FALSE(SelectRotationBound(root, {}).has_value());
}

TEST_CASE("Inflated root bounds use the visual geometry center", "[bounds]") {
    const std::array samples {Bound(10.0F, 2.0F), Bound(14.0F, 2.0F)};
    const auto result = SelectRotationBound(Bound(50.0F, 100.0F), samples);
    REQUIRE(result.has_value());
    CHECK(result->center.x == Catch::Approx(12.0F));
    CHECK(result->radius == Catch::Approx(4.0F));
}

TEST_CASE("Radius outliers are trimmed only with enough geometry", "[bounds]") {
    const auto root = Bound(0.0F, 100.0F);
    SECTION("Four samples allow trimming") {
        const std::array samples {Bound(10.0F, 1.0F), Bound(10.0F, 2.0F), Bound(10.0F, 3.0F), Bound(50.0F, 80.0F)};
        const auto result = SelectRotationBound(root, samples);
        REQUIRE(result.has_value());
        CHECK(result->center.x == Catch::Approx(10.0F));
        CHECK(result->radius == Catch::Approx(3.0F));
    }
    SECTION("A large part is retained in a small sample") {
        const std::array samples {Bound(10.0F, 2.0F), Bound(50.0F, 80.0F)};
        const auto result = SelectRotationBound(root, samples);
        REQUIRE(result.has_value());
        CHECK(result->radius == Catch::Approx(80.0F));
    }
}

TEST_CASE("Invalid geometry does not influence the center or trimming threshold", "[bounds]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::array samples {Bound(10.0F, 2.0F), Bound(nan, 2.0F), Bound(0.0F, nan), Bound(0.0F, -1.0F)};
    const auto result = SelectRotationBound(Bound(0.0F, 100.0F), samples);
    REQUIRE(result.has_value());
    CHECK(result->center.x == Catch::Approx(10.0F));
    CHECK_FALSE(SelectRotationBound(Bound(nan, 100.0F), samples).has_value());
}

TEST_CASE("Outlier trimming uses the upper median and includes the radius threshold", "[bounds]") {
    const auto root = Bound(0.0F, 200.0F);
    SECTION("An even sample uses the upper middle radius") {
        const std::array samples {Bound(10.0F, 30.0F), Bound(10.0F, 1.0F), Bound(10.0F, 8.0F), Bound(10.0F, 2.0F)};
        const auto result = SelectRotationBound(root, samples);
        REQUIRE(result.has_value());
        CHECK(result->radius == Catch::Approx(30.0F));
    }
    SECTION("A radius equal to the outlier threshold is removed") {
        const std::array samples {Bound(10.0F, 1.0F), Bound(10.0F, 2.0F), Bound(10.0F, 3.0F), Bound(50.0F, 25.0F)};
        const auto result = SelectRotationBound(root, samples);
        REQUIRE(result.has_value());
        CHECK(result->center.x == Catch::Approx(10.0F));
        CHECK(result->radius == Catch::Approx(3.0F));
    }
}

TEST_CASE("Trimming preserves the geometry merge order", "[bounds]") {
    const std::array samples {
        Bound(0.0F, 1.0F),
        Bound(10.0F, 1.0F),
        RE::NiBound {.center = {5.0F, 10.0F, 0.0F}, .radius = 1.0F},
        Bound(100.0F, 150.0F)
    };
    const auto result = SelectRotationBound(Bound(0.0F, 1000.0F), samples);
    REQUIRE(result.has_value());
    CHECK(result->center.x == Catch::Approx(5.0F));
    CHECK(result->center.y == Catch::Approx(2.5F));
    CHECK(result->radius == Catch::Approx(8.5F));
}
