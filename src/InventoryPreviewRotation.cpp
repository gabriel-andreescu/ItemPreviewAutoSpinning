#include "InventoryPreviewRotation.h"
#include "HookUtil.h"
#include "PCH.h" // IWYU pragma: keep
#include "RotationBounds.h"

#include <RE/B/BSTArray.h>
#include <RE/I/InterfaceLightSchemes.h>
#include <RE/I/Inventory3DManager.h>
#include <RE/N/NiPoint2.h>
#include <RE/T/TESForm.h>
#include <REL/Module.h>
#include <REL/Pattern.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <spdlog/fmt/bin_to_hex.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {
inline constexpr std::size_t kExistingBranchPatchSize {5};
inline constexpr std::size_t kApplyRotationPatchSize {18};

constexpr std::array<std::byte, kApplyRotationPatchSize> kApplyRotationPrologueSE {
    std::byte {0x48},
    std::byte {0x8B},
    std::byte {0xC4},
    std::byte {0x55},
    std::byte {0x48},
    std::byte {0x8D},
    std::byte {0xA8},
    std::byte {0x68},
    std::byte {0xFE},
    std::byte {0xFF},
    std::byte {0xFF},
    std::byte {0x48},
    std::byte {0x81},
    std::byte {0xEC},
    std::byte {0x90},
    std::byte {0x02},
    std::byte {0x00},
    std::byte {0x00},
};

constexpr std::array<std::byte, kApplyRotationPatchSize> kApplyRotationPrologueAEGOG {
    std::byte {0x48},
    std::byte {0x8B},
    std::byte {0xC4},
    std::byte {0x55},
    std::byte {0x48},
    std::byte {0x8D},
    std::byte {0xA8},
    std::byte {0x18},
    std::byte {0xFF},
    std::byte {0xFF},
    std::byte {0xFF},
    std::byte {0x48},
    std::byte {0x81},
    std::byte {0xEC},
    std::byte {0xE0},
    std::byte {0x01},
    std::byte {0x00},
    std::byte {0x00},
};

using ApplyRotationFunction = void (*)(RE::Inventory3DManager*, RE::NiPoint2*);

[[nodiscard]] RE::LoadedInventoryModel* GetCurrentLoadedModel(RE::Inventory3DManager& a_manager) {
    auto& loadedModels = a_manager.GetRuntimeData().loadedModels;
    if (loadedModels.empty()) {
        return nullptr;
    }

    return &loadedModels.back();
}

[[nodiscard]] const RE::TESForm* GetCurrentItemBase(const RE::Inventory3DManager& a_manager) {
    if (REL::Module::IsVR()) {
        // VR uses 0x48-byte entries and stores the array count at manager + 0x258.
        struct LoadedInventoryModelVR {
            RE::TESForm* itemBase;
            std::array<std::byte, 0x40> remaining;
        };
        static_assert(sizeof(LoadedInventoryModelVR) == 0x48);

        const auto& models = REL::RelocateMember<const RE::BSTSmallArray<LoadedInventoryModelVR, 7>>(&a_manager, 0x58);
        return models.empty() ? nullptr : models.back().itemBase;
    }

    const auto& models = a_manager.GetRuntimeData().loadedModels;
    return models.empty() ? nullptr : models.back().itemBase;
}

void LogUnsupportedApplyRotationPrologue(const std::uintptr_t a_address, std::span<const std::byte> a_bytes) {
    SKSE::log::error(
        "Unsupported Inventory3DManager preview rotation prologue at {:X}: {}",
        a_address,
        spdlog::to_hex(a_bytes.begin(), a_bytes.end())
    );
}

struct RotationHook {
    [[nodiscard]] static bool Install() {
        REL::Relocation<std::byte*> const target {REL::VariantID(50902, 51778, 0x8B65F0)};
        const auto* targetBytes = target.get();
        const auto address = target.address();
        auto& trampoline = SKSE::GetTrampoline();

        if (REL::make_pattern<"E9">().match(address)) {
            func = trampoline.write_branch<kExistingBranchPatchSize>(address, Thunk);

            SKSE::log::info("Chained existing Inventory3DManager preview rotation hook");
            return true;
        }

        if (HookUtil::HasExpectedPrologue(targetBytes, kApplyRotationPrologueSE)
            || HookUtil::HasExpectedPrologue(targetBytes, kApplyRotationPrologueAEGOG)) {
            if (!HookUtil::HookFunctionPrologue<RotationHook, kApplyRotationPatchSize>(address, targetBytes)) {
                SKSE::log::error("Failed to write Inventory3DManager preview rotation hook");
                return false;
            }

            SKSE::log::info("Installed Inventory3DManager preview rotation hook");
            return true;
        }

        LogUnsupportedApplyRotationPrologue(address, std::span<const std::byte> {targetBytes, kApplyRotationPatchSize});
        return false;
    }

    static void Thunk(RE::Inventory3DManager* a_manager, RE::NiPoint2* a_rotationDelta) {
        if ((a_manager == nullptr) || !InventoryPreviewRotation::ShouldHandle(*a_manager)) {
            func(a_manager, a_rotationDelta);
            return;
        }

        const auto* loadedModel = GetCurrentLoadedModel(*a_manager);
        const bool appliedSanitizedCenter = (loadedModel != nullptr)
                                            && RotationBounds::ApplySanitizedRotationCenter(*loadedModel);

        func(a_manager, a_rotationDelta);

        if (appliedSanitizedCenter) {
            RotationBounds::RestoreFullRotationBound(*loadedModel);
        }
    }

    static inline REL::Relocation<ApplyRotationFunction> func;
};

}

void InventoryPreviewRotation::Install() {
    // VR rotates a parent node and does not use the model bound as its pivot.
    if (REL::Module::IsVR()) {
        return;
    }
    if (!RotationHook::Install()) {
        SKSE::log::warn("Preview centering hook unavailable. Using vanilla preview rotation");
    }
}

void InventoryPreviewRotation::Apply(RE::Inventory3DManager& a_manager, const RE::NiPoint2& a_rotationDelta) {
    static REL::Relocation<ApplyRotationFunction> const applyRotation {REL::VariantID(50902, 51778, 0x8B65F0)};

    auto delta = a_rotationDelta;
    applyRotation(&a_manager, &delta);
}

bool InventoryPreviewRotation::ShouldHandle(const RE::Inventory3DManager& a_manager) noexcept {
    if (a_manager.currentLightScheme != RE::INTERFACE_LIGHT_SCHEME::kInventory) {
        return false;
    }

    const auto* itemBase = GetCurrentItemBase(a_manager);
    return (itemBase != nullptr) && itemBase->IsInventoryObject();
}
