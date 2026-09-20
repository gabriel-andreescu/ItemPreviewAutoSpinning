#include "AutoSpin.h"
#include "HookUtil.h"
#include "InventoryPreviewRotation.h"
#include "PCH.h" // IWYU pragma: keep
#include "Settings.h"
#include "SpinState.h"

#include <RE/B/BSTimer.h>
#include <RE/I/INISettingCollection.h>
#include <RE/I/Inventory3DManager.h>
#include <RE/M/MouseMoveEvent.h>
#include <RE/N/NiPoint2.h>
#include <RE/S/Setting.h>
#include <REL/Module.h>
#include <REL/Pattern.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <spdlog/fmt/bin_to_hex.h>

#include <RE/Offsets_VTABLE.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {
inline constexpr std::size_t kInventory3DManagerRenderPatchSize {6};
inline constexpr std::size_t kExistingBranchPatchSize {5};

constexpr std::array<std::byte, kInventory3DManagerRenderPatchSize> kInventory3DManagerRenderPrologue {
    std::byte {0x40},
    std::byte {0x53},
    std::byte {0x48},
    std::byte {0x83},
    std::byte {0xEC},
    std::byte {0x20},
};

void LogUnsupportedInventory3DManagerRenderPrologue(std::uintptr_t a_address, const std::byte* a_bytes) {
    const std::span prologue {a_bytes, kInventory3DManagerRenderPatchSize};
    SKSE::log::critical(
        "Unsupported Inventory3DManager::Render prologue at {:X}: {}",
        a_address,
        spdlog::to_hex(prologue.begin(), prologue.end())
    );
}

[[nodiscard]] std::size_t GetMouseSlot() {
    if (REL::Module::IsVR()) {
        return 7;
    }
    if (REL::Module::IsAE() && REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_7_99)) {
        return 6;
    }
    return 4;
}

[[nodiscard]] float GetRealTimeDelta() {
    const auto* timer = RE::BSTimer::GetSingleton();
    if (timer == nullptr) {
        return 0.0F;
    }

    return timer->realTimeDelta;
}

[[nodiscard]] bool IsMouseRotationActive() {
    static REL::Relocation<bool*> const bMouseRotation {RELOCATION_ID(519620, 406167)};
    return *bMouseRotation;
}

SpinState& GetSpinState() {
    static SpinState state;
    return state;
}

void CaptureManualVelocity(const RE::MouseMoveEvent& a_event) {
    static RE::Setting const* mouseSpeedSetting = nullptr;
    if (mouseSpeedSetting == nullptr) {
        if (auto* collection = RE::INISettingCollection::GetSingleton()) {
            mouseSpeedSetting = collection->GetSetting("fInventory3DItemRotMouseSpeed:Interface");
        }
    }

    if (mouseSpeedSetting == nullptr) {
        return;
    }

    const float mouseSpeed = mouseSpeedSetting->GetFloat();
    GetSpinState().CaptureVelocity(
        {-static_cast<float>(a_event.mouseInputX) * mouseSpeed, static_cast<float>(a_event.mouseInputY) * mouseSpeed}
    );
}

void ApplyAutoSpin(RE::Inventory3DManager& a_manager) {
    const auto rotation = GetSpinState().Update(
        Settings::GetSingleton()->GetValues(),
        IsMouseRotationActive(),
        GetRealTimeDelta()
    );
    if (rotation.x != 0.0F || rotation.y != 0.0F) {
        InventoryPreviewRotation::Apply(a_manager, rotation);
    }
}

struct RenderHook {
    [[nodiscard]] static bool Install() {
        REL::Relocation<std::byte*> const target {RELOCATION_ID(50882, 51755)};
        const auto* targetBytes = target.get();
        const auto address = target.address();
        auto& trampoline = SKSE::GetTrampoline();

        if (REL::make_pattern<"E9">().match(address)) {
            func = trampoline.write_branch<kExistingBranchPatchSize>(address, Thunk);

            SKSE::log::info("Chained existing Inventory3DManager::Render hook");
            return true;
        }

        if (HookUtil::HasExpectedPrologue(targetBytes, kInventory3DManagerRenderPrologue)) {
            if (!HookUtil::HookFunctionPrologue<RenderHook, kInventory3DManagerRenderPatchSize>(address, targetBytes)) {
                SKSE::log::critical("Failed to write Inventory3DManager::Render hook");
                return false;
            }

            SKSE::log::info("Installed Inventory3DManager::Render hook");
            return true;
        }

        LogUnsupportedInventory3DManagerRenderPrologue(address, targetBytes);
        return false;
    }

    static void Thunk(RE::Inventory3DManager* a_manager) {
        if (a_manager == nullptr) {
            GetSpinState().Reset();
            return;
        }

        if (InventoryPreviewRotation::ShouldHandle(*a_manager)) {
            ApplyAutoSpin(*a_manager);
        } else {
            GetSpinState().Reset();
        }
        func(a_manager);
    }

    static inline REL::Relocation<decltype(Thunk)> func;
};

struct MouseMoveHook {
    static bool Thunk(RE::Inventory3DManager* a_manager, RE::MouseMoveEvent* a_event) {
        const bool processed = func(a_manager, a_event);
        if (processed
            && (a_manager != nullptr)
            && (a_event != nullptr)
            && InventoryPreviewRotation::ShouldHandle(*a_manager)
            && IsMouseRotationActive()) {
            CaptureManualVelocity(*a_event);
        }

        return processed;
    }

    static inline REL::Relocation<decltype(Thunk)> func;
};
}

void AutoSpin::Install() {
    InventoryPreviewRotation::Install();

    if (!RenderHook::Install()) {
        SKSE::stl::report_and_fail("Failed to install Inventory3DManager::Render hook");
    }

    const auto mouseSlot = GetMouseSlot();
    REL::Relocation<std::uintptr_t> vtable {RE::VTABLE_Inventory3DManager[0]};
    MouseMoveHook::func = vtable.write_vfunc(mouseSlot, MouseMoveHook::Thunk);
    SKSE::log::info("Preview hooks installed for {} (mouse slot {})", REL::Module::get().version(), mouseSlot);
}
