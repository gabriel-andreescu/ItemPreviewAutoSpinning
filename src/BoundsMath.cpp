#include "BoundsMath.h"
#include "PCH.h" // IWYU pragma: keep
#include <RE/N/NiBound.h>
#include <RE/N/NiPoint3.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <vector>
namespace {
inline constexpr std::size_t kMinGeometryBoundsForSanitization {1};
inline constexpr std::size_t kMinGeometryBoundsForOutlierTrimming {4};
inline constexpr float kMinUsableBoundRadius {0.001F};
inline constexpr float kMinOutlierBoundRadius {25.0F};
inline constexpr float kOutlierRadiusRatio {4.0F};
inline constexpr float kMaxSanitizedRootRadiusRatio {0.85F};

[[nodiscard]] bool IsFinite(const RE::NiPoint3& a_point) {
    return std::isfinite(a_point.x) && std::isfinite(a_point.y) && std::isfinite(a_point.z);
}

[[nodiscard]] bool IsUsableBound(const RE::NiBound& a_bound) {
    return std::isfinite(a_bound.radius) && a_bound.radius > kMinUsableBoundRadius && IsFinite(a_bound.center);
}

[[nodiscard]] float GetMedian(std::vector<float> a_values) {
    const auto middle = a_values.begin() + static_cast<std::ptrdiff_t>(a_values.size() / 2);
    std::ranges::nth_element(a_values, middle);
    return *middle;
}

[[nodiscard]] RE::NiBound MergeBounds(const RE::NiBound& a_lhs, const RE::NiBound& a_rhs) {
    if (!IsUsableBound(a_lhs)) {
        return a_rhs;
    }

    if (!IsUsableBound(a_rhs)) {
        return a_lhs;
    }

    const float distance = a_lhs.center.GetDistance(a_rhs.center);
    if (distance + a_rhs.radius <= a_lhs.radius) {
        return a_lhs;
    }

    if (distance + a_lhs.radius <= a_rhs.radius) {
        return a_rhs;
    }

    RE::NiBound merged {};
    merged.radius = (distance + a_lhs.radius + a_rhs.radius) * 0.5F;
    if (distance > std::numeric_limits<float>::epsilon()) {
        merged.center = a_lhs.center + ((a_rhs.center - a_lhs.center) * ((merged.radius - a_lhs.radius) / distance));
    } else {
        merged.center = a_lhs.center;
    }

    return merged;
}

[[nodiscard]] RE::NiBound MergeBounds(std::span<const RE::NiBound> a_samples) {
    RE::NiBound merged {};
    for (const auto& sample : a_samples) {
        merged = MergeBounds(merged, sample);
    }

    return merged;
}

[[nodiscard]] std::vector<float> GetRadii(std::span<const RE::NiBound> a_samples) {
    std::vector<float> radii;
    radii.reserve(a_samples.size());

    for (const auto& sample : a_samples) {
        radii.push_back(sample.radius);
    }

    return radii;
}

[[nodiscard]] bool IsRadiusOutlier(const RE::NiBound& a_sample, const float a_medianRadius) {
    const float minOutlierRadius = std::max(kMinOutlierBoundRadius, a_medianRadius * kOutlierRadiusRatio);
    return a_sample.radius >= minOutlierRadius;
}

[[nodiscard]] std::vector<RE::NiBound> RemoveRadiusOutliers(std::span<const RE::NiBound> a_samples) {
    std::vector<RE::NiBound> usable;
    usable.reserve(a_samples.size());
    std::ranges::copy_if(a_samples, std::back_inserter(usable), IsUsableBound);
    if (usable.size() < kMinGeometryBoundsForOutlierTrimming) {
        return usable;
    }
    const float medianRadius = GetMedian(GetRadii(usable));
    std::erase_if(usable, [medianRadius](const auto& a_sample) { return IsRadiusOutlier(a_sample, medianRadius); });
    return usable;
}

}
std::optional<RE::NiBound> SelectRotationBound(const RE::NiBound& a_root, std::span<const RE::NiBound> a_samples) {
    if (a_samples.size() < kMinGeometryBoundsForSanitization) {
        return std::nullopt;
    }

    if (!IsUsableBound(a_root)) {
        return std::nullopt;
    }

    auto kept = RemoveRadiusOutliers(a_samples);
    if (kept.empty()) {
        return std::nullopt;
    }

    const auto sanitizedBound = MergeBounds(kept);
    if (!IsUsableBound(sanitizedBound)) {
        return std::nullopt;
    }

    if (sanitizedBound.radius >= (a_root.radius * kMaxSanitizedRootRadiusRatio)) {
        return std::nullopt;
    }

    return sanitizedBound;
}
