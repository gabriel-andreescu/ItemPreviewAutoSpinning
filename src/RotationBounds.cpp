#include "RotationBounds.h"

#include "RE/B/BSGeometry.h"
#include "RE/B/BSVisit.h"
#include "RE/I/Inventory3DManager.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiBound.h"
#include "RE/N/NiPoint3.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace {
inline constexpr std::size_t kMinGeometryBoundsForSanitization {1};
inline constexpr std::size_t kMinGeometryBoundsForOutlierTrimming {4};
inline constexpr float kMinUsableBoundRadius {0.001F};
inline constexpr float kMinOutlierBoundRadius {25.0F};
inline constexpr float kOutlierRadiusRatio {4.0F};
inline constexpr float kMaxSanitizedRootRadiusRatio {0.85F};

struct GeometryBoundSample {
    RE::NiBound bound;
};

[[nodiscard]] bool IsFinite(const RE::NiPoint3& a_point) {
    return std::isfinite(a_point.x) && std::isfinite(a_point.y) && std::isfinite(a_point.z);
}

[[nodiscard]] bool IsUsableBound(const RE::NiBound& a_bound) {
    return std::isfinite(a_bound.radius) && a_bound.radius > kMinUsableBoundRadius && IsFinite(a_bound.center);
}

[[nodiscard]] bool IsNonVisualGeometry(RE::BSGeometry& a_geometry) {
    const auto type = a_geometry.GetType();
    if (type == RE::BSGeometry::Type::kParticles || type == RE::BSGeometry::Type::kStripParticles) {
        return true;
    }

    if (type == RE::BSGeometry::Type::kParticleShaderDynamicTriShape) {
        return true;
    }

    return type == RE::BSGeometry::Type::kLines || type == RE::BSGeometry::Type::kDynamicLines;
}

[[nodiscard]] float GetMedian(std::vector<float> a_values) {
    std::ranges::sort(a_values);
    return a_values[a_values.size() / 2];
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

[[nodiscard]] RE::NiBound MergeBounds(const std::vector<GeometryBoundSample>& a_samples) {
    RE::NiBound merged {};
    for (const auto& sample : a_samples) {
        merged = MergeBounds(merged, sample.bound);
    }

    return merged;
}

[[nodiscard]] std::vector<GeometryBoundSample> CollectGeometryBounds(RE::NiAVObject& a_root) {
    std::vector<GeometryBoundSample> samples;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry) {
        if (a_geometry && !IsNonVisualGeometry(*a_geometry) && IsUsableBound(a_geometry->worldBound)) {
            samples.push_back({
                .bound = a_geometry->worldBound,
            });
        }

        return RE::BSVisit::BSVisitControl::kContinue;
    });

    return samples;
}

[[nodiscard]] std::vector<float> GetRadii(const std::vector<GeometryBoundSample>& a_samples) {
    std::vector<float> radii;
    radii.reserve(a_samples.size());

    for (const auto& sample : a_samples) {
        radii.push_back(sample.bound.radius);
    }

    return radii;
}

[[nodiscard]] bool IsRadiusOutlier(const GeometryBoundSample& a_sample, const float a_medianRadius) {
    const float minOutlierRadius = std::max(kMinOutlierBoundRadius, a_medianRadius * kOutlierRadiusRatio);
    return a_sample.bound.radius >= minOutlierRadius;
}

[[nodiscard]] std::vector<GeometryBoundSample> RemoveRadiusOutliers(const std::vector<GeometryBoundSample>& a_samples) {
    if (a_samples.empty()) {
        return {};
    }

    const float medianRadius = GetMedian(GetRadii(a_samples));
    std::vector<GeometryBoundSample> kept;
    kept.reserve(a_samples.size());

    const bool canTrimOutliers = a_samples.size() >= kMinGeometryBoundsForOutlierTrimming;
    for (const auto& sample : a_samples) {
        if (canTrimOutliers && IsRadiusOutlier(sample, medianRadius)) {
            continue;
        }

        kept.push_back(sample);
    }
    return kept;
}

[[nodiscard]] std::optional<RE::NiBound> BuildSanitizedGeometryBound(RE::NiAVObject& a_root) {
    auto samples = CollectGeometryBounds(a_root);
    if (samples.size() < kMinGeometryBoundsForSanitization) {
        return std::nullopt;
    }

    if (!IsUsableBound(a_root.worldBound)) {
        return std::nullopt;
    }

    auto kept = RemoveRadiusOutliers(samples);
    if (kept.empty()) {
        return std::nullopt;
    }

    const auto sanitizedBound = MergeBounds(kept);
    if (!IsUsableBound(sanitizedBound)) {
        return std::nullopt;
    }

    if (sanitizedBound.radius >= (a_root.worldBound.radius * kMaxSanitizedRootRadiusRatio)) {
        return std::nullopt;
    }

    return sanitizedBound;
}
}

bool RotationBounds::ApplySanitizedRotationCenter(RE::LoadedInventoryModel& a_loadedModel) {
    auto* model = a_loadedModel.spModel.get();
    if (!model) {
        return false;
    }

    auto sanitizedBound = BuildSanitizedGeometryBound(*model);
    if (!sanitizedBound) {
        return false;
    }

    model->worldBound.center = sanitizedBound->center;
    return true;
}

void RotationBounds::RestoreFullRotationBound(RE::LoadedInventoryModel& a_loadedModel) {
    if (auto* model = a_loadedModel.spModel.get()) {
        model->UpdateWorldBound();
    }
}
