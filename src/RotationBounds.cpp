#include "RotationBounds.h"
#include "BoundsMath.h"
#include "PCH.h" // IWYU pragma: keep
#include <RE/B/BSGeometry.h>
#include <RE/B/BSVisit.h>
#include <RE/I/Inventory3DManager.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiBound.h>
#include <RE/N/NiSmartPointer.h>
#include <optional>
#include <vector>
namespace {
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

[[nodiscard]] std::vector<RE::NiBound> CollectGeometryBounds(RE::NiAVObject& a_root) {
    std::vector<RE::NiBound> samples;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry) {
        if (a_geometry && !IsNonVisualGeometry(*a_geometry)) {
            samples.push_back(a_geometry->worldBound);
        }

        return RE::BSVisit::BSVisitControl::kContinue;
    });

    return samples;
}

}
bool RotationBounds::ApplySanitizedRotationCenter(const RE::LoadedInventoryModel& a_loadedModel) {
    auto* model = a_loadedModel.spModel.get();
    if (model == nullptr) {
        return false;
    }

    const auto samples = CollectGeometryBounds(*model);
    const auto sanitizedBound = SelectRotationBound(model->worldBound, samples);
    if (!sanitizedBound) {
        return false;
    }

    model->worldBound.center = sanitizedBound->center;
    return true;
}

void RotationBounds::RestoreFullRotationBound(const RE::LoadedInventoryModel& a_loadedModel) {
    if (auto* model = a_loadedModel.spModel.get()) {
        model->UpdateWorldBound();
    }
}
