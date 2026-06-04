#pragma once

namespace RE {
struct LoadedInventoryModel;
}

namespace RotationBounds {
[[nodiscard]] bool ApplySanitizedRotationCenter(RE::LoadedInventoryModel& a_loadedModel);
void RestoreFullRotationBound(RE::LoadedInventoryModel& a_loadedModel);
}
