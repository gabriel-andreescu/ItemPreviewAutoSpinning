#include "InventoryPreviewRotation.h"

#include "HookUtil.h"
#include "RotationBounds.h"

#include "RE/I/Inventory3DManager.h"
#include "RE/N/NiPoint2.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>

namespace {
inline constexpr std::size_t kExistingBranchPatchSize {5};
inline constexpr std::size_t kApplyRotationPatchSize {18};
inline constexpr std::size_t kApplyRotationPatchSizeVR {6};

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

constexpr std::array<std::byte, kApplyRotationPatchSizeVR> kApplyRotationPrologueVR {
    std::byte {0x40},
    std::byte {0x53},
    std::byte {0x48},
    std::byte {0x83},
    std::byte {0xEC},
    std::byte {0x70},
};

using ApplyRotation_t = void (*)(RE::Inventory3DManager*, RE::NiPoint2*);

[[nodiscard]] RE::LoadedInventoryModel* GetCurrentLoadedModel(RE::Inventory3DManager& a_manager) {
    auto& loadedModels = a_manager.GetRuntimeData().loadedModels;
    if (loadedModels.empty()) {
        return nullptr;
    }

    return &loadedModels.back();
}

[[nodiscard]] std::string FormatBytes(std::span<const std::byte> a_bytes) {
    if (a_bytes.empty()) {
        return {};
    }

    constexpr std::array<char, 16> hexDigits {
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        'A',
        'B',
        'C',
        'D',
        'E',
        'F',
    };

    std::string formatted;
    formatted.reserve((a_bytes.size() * 3) - 1);

    for (const auto byte : a_bytes) {
        if (!formatted.empty()) {
            formatted.push_back(' ');
        }

        const auto value = std::to_integer<unsigned>(byte);
        formatted.push_back(hexDigits[(value >> 4) & 0xF]);
        formatted.push_back(hexDigits[value & 0xF]);
    }

    return formatted;
}

void LogUnsupportedApplyRotationPrologue(const std::uintptr_t a_address, std::span<const std::byte> a_bytes) {
    logger::critical(
        "Hooks: Inventory3DManager preview rotation hook skipped | reason=unsupportedPrologue | address={:X} | byteCount={} | bytes={}",
        a_address,
        a_bytes.size(),
        FormatBytes(a_bytes)
    );
}

struct Inventory3DManager_ApplyRotation {
    [[nodiscard]] static bool Install() {
        // SE:  Inventory3DManager preview rotation helper 50902 -> 140888C20.
        // AE:  Inventory3DManager preview rotation helper 51778 -> 140928C40.
        // GOG: Inventory3DManager preview rotation helper 51778 -> 14092AC80.
        // VR:  Inventory3DManager preview rotation helper 1408B65F0.
        REL::Relocation<std::byte*> target {REL::VariantID(50902, 51778, 0x8B65F0)};
        const auto* targetBytes = target.get();
        const auto address = target.address();
        auto& trampoline = SKSE::GetTrampoline();

        if (REL::make_pattern<"E9">().match(address)) {
            func = trampoline.write_branch<kExistingBranchPatchSize>(address, thunk);

            logger::warn("Hooks: Inventory3DManager preview rotation hook chained | reason=existingBranch | branch=E9");
            return true;
        }

        if (REL::Module::IsVR()) {
            if (HookUtil::HasExpectedPrologue(targetBytes, kApplyRotationPrologueVR)) {
                HookUtil::HookFunctionPrologue<Inventory3DManager_ApplyRotation, kApplyRotationPatchSizeVR>(
                    address,
                    targetBytes
                );

                logger::info("Hooks: Inventory3DManager preview rotation hook installed");
                return true;
            }

            LogUnsupportedApplyRotationPrologue(
                address,
                std::span<const std::byte> {targetBytes, kApplyRotationPatchSizeVR}
            );
            return false;
        }

        if (HookUtil::HasExpectedPrologue(targetBytes, kApplyRotationPrologueSE)
            || HookUtil::HasExpectedPrologue(targetBytes, kApplyRotationPrologueAEGOG)) {
            HookUtil::HookFunctionPrologue<Inventory3DManager_ApplyRotation, kApplyRotationPatchSize>(
                address,
                targetBytes
            );

            logger::info("Hooks: Inventory3DManager preview rotation hook installed");
            return true;
        }

        LogUnsupportedApplyRotationPrologue(address, std::span<const std::byte> {targetBytes, kApplyRotationPatchSize});
        return false;
    }

    static void thunk(RE::Inventory3DManager* a_manager, RE::NiPoint2* a_rotationDelta) {
        if (!a_manager || !InventoryPreviewRotation::ShouldHandle(*a_manager)) {
            func(a_manager, a_rotationDelta);
            return;
        }

        auto* loadedModel = GetCurrentLoadedModel(*a_manager);
        const bool appliedSanitizedCenter = loadedModel && RotationBounds::ApplySanitizedRotationCenter(*loadedModel);

        func(a_manager, a_rotationDelta);

        if (appliedSanitizedCenter) {
            RotationBounds::RestoreFullRotationBound(*loadedModel);
        }
    }

    static inline REL::Relocation<ApplyRotation_t> func;
};

}

void InventoryPreviewRotation::Install() {
    if (!Inventory3DManager_ApplyRotation::Install()) {
        stl::report_and_fail("Failed to install Inventory3DManager preview rotation hook"sv);
    }
}

void InventoryPreviewRotation::Apply(RE::Inventory3DManager& a_manager, const RE::NiPoint2& a_rotationDelta) {
    static REL::Relocation<ApplyRotation_t> applyRotation {REL::VariantID(50902, 51778, 0x8B65F0)};

    auto rotationDelta = a_rotationDelta;
    applyRotation(&a_manager, &rotationDelta);
}

bool InventoryPreviewRotation::ShouldHandle(const RE::Inventory3DManager& a_manager) noexcept {
    return a_manager.currentLightScheme == RE::INTERFACE_LIGHT_SCHEME::kInventory;
}
