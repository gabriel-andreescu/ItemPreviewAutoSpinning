#pragma once

namespace RE {
class Inventory3DManager;
class NiPoint2;
}

namespace InventoryPreviewRotation {
void Install();
void Apply(RE::Inventory3DManager& a_manager, const RE::NiPoint2& a_rotationDelta);

[[nodiscard]] bool ShouldHandle(const RE::Inventory3DManager& a_manager) noexcept;
}
