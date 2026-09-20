#pragma once

namespace RE {
struct LoadedInventoryModel;
}

namespace RotationBounds {
[[nodiscard]] bool ApplySanitizedRotationCenter(const RE::LoadedInventoryModel& a_loadedModel);
void RestoreFullRotationBound(const RE::LoadedInventoryModel& a_loadedModel);
}
