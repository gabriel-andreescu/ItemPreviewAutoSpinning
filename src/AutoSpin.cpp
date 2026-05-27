#include "AutoSpin.h"

#include "Settings.h"

#include "RE/B/BSTimer.h"
#include "RE/I/INISettingCollection.h"
#include "RE/I/Inventory3DManager.h"
#include "RE/I/InventoryMenu.h"
#include "RE/M/MouseMoveEvent.h"
#include "RE/N/NiPoint2.h"
#include "RE/Offsets_VTABLE.h"
#include "RE/S/Setting.h"

#include <algorithm>
#include <cmath>

namespace {
inline constexpr float kMaxFrameDelta {1.0F / 20.0F};
inline constexpr float kManualVelocitySmoothing {0.45F};
inline constexpr float kManualSpinStopRatio {0.05F};
inline constexpr float kManualSpinMinReleaseSpeed {0.12F};
inline constexpr float kManualSpinStopSpeed {0.02F};
inline constexpr float kManualSpinMaxSpeed {8.0F};

using ApplyInventoryPreviewRotation_t = void (*)(RE::Inventory3DManager*, RE::NiPoint2*);

float g_resumeDelayRemaining {0.0F};
RE::NiPoint2 g_dragVelocity {};
RE::NiPoint2 g_manualSpinVelocity {};
bool g_hasDragVelocity {false};
bool g_wasMouseRotationActive {false};

[[nodiscard]] float GetRealTimeDelta() {
    auto* timer = RE::BSTimer::GetSingleton();
    if (!timer) {
        return 0.0F;
    }

    const auto realTimeDelta = REL::RelocateMember<float>(timer, 0x1C, 0x18);
    return std::clamp(realTimeDelta, 0.0F, kMaxFrameDelta);
}

[[nodiscard]] bool IsMouseRotationActive() {
    static REL::Relocation<bool*> bMouseRotation {RELOCATION_ID(519620, 406167)};
    return *bMouseRotation;
}

[[nodiscard]] float GetSpeed(const RE::NiPoint2& a_velocity) {
    return std::sqrt((a_velocity.x * a_velocity.x) + (a_velocity.y * a_velocity.y));
}

void ClampVelocity(RE::NiPoint2& a_velocity, const float a_maxSpeed) {
    const float speed = GetSpeed(a_velocity);
    if (speed <= a_maxSpeed) {
        return;
    }

    const float scale = a_maxSpeed / speed;
    a_velocity.x *= scale;
    a_velocity.y *= scale;
}

void ApplyRotation(RE::Inventory3DManager& a_manager, const RE::NiPoint2& a_rotationDelta) {
    // SE: 50902 -> 140888C20. AE: 51778 -> 140928C40. VR: 1408B65F0.
    static REL::Relocation<ApplyInventoryPreviewRotation_t> applyRotation {REL::VariantID(50902, 51778, 0x8B65F0)};

    auto rotationDelta = a_rotationDelta;
    applyRotation(&a_manager, &rotationDelta);
}

void CaptureManualVelocity(const RE::MouseMoveEvent& a_event) {
    static RE::Setting* mouseSpeedSetting = nullptr;
    if (!mouseSpeedSetting) {
        if (auto* collection = RE::INISettingCollection::GetSingleton()) {
            mouseSpeedSetting = collection->GetSetting("fInventory3DItemRotMouseSpeed:Interface");
        }
    }

    if (!mouseSpeedSetting) {
        return;
    }

    const float mouseSpeed = mouseSpeedSetting->GetFloat();
    RE::NiPoint2 velocity {
        -static_cast<float>(a_event.mouseInputX) * mouseSpeed,
        static_cast<float>(a_event.mouseInputY) * mouseSpeed
    };

    if (GetSpeed(velocity) <= 0.0F) {
        return;
    }

    if (g_hasDragVelocity) {
        g_dragVelocity.x = std::lerp(g_dragVelocity.x, velocity.x, kManualVelocitySmoothing);
        g_dragVelocity.y = std::lerp(g_dragVelocity.y, velocity.y, kManualVelocitySmoothing);
    } else {
        g_dragVelocity = velocity;
        g_hasDragVelocity = true;
    }
}

void StartManualSpin(const Settings& a_settings) {
    if (!a_settings.manualSpinAfterDrag || !g_hasDragVelocity || a_settings.manualSpinStrength <= 0.0F) {
        g_hasDragVelocity = false;
        g_dragVelocity = {};
        return;
    }

    g_manualSpinVelocity.x = g_dragVelocity.x * a_settings.manualSpinStrength;
    g_manualSpinVelocity.y = g_dragVelocity.y * a_settings.manualSpinStrength;
    ClampVelocity(g_manualSpinVelocity, kManualSpinMaxSpeed);

    g_hasDragVelocity = false;
    g_dragVelocity = {};

    if (GetSpeed(g_manualSpinVelocity) < kManualSpinMinReleaseSpeed) {
        g_manualSpinVelocity = {};
    }
}

[[nodiscard]] bool ApplyManualSpin(
    RE::Inventory3DManager& a_manager,
    const Settings& a_settings,
    const float a_frameDelta
) {
    if (GetSpeed(g_manualSpinVelocity) < kManualSpinStopSpeed) {
        g_manualSpinVelocity = {};
        return false;
    }

    RE::NiPoint2 rotationDelta {g_manualSpinVelocity.x * a_frameDelta, g_manualSpinVelocity.y * a_frameDelta};
    ApplyRotation(a_manager, rotationDelta);

    const float decay = std::pow(kManualSpinStopRatio, a_frameDelta / a_settings.manualSpinDuration);
    g_manualSpinVelocity.x *= decay;
    g_manualSpinVelocity.y *= decay;

    return true;
}

void ApplyAutoSpin() {
    const float frameDelta = GetRealTimeDelta();
    if (frameDelta <= 0.0F) {
        return;
    }

    const auto* settings = Settings::GetSingleton();
    auto* manager = RE::Inventory3DManager::GetSingleton();
    if (!manager) {
        g_wasMouseRotationActive = false;
        g_hasDragVelocity = false;
        g_dragVelocity = {};
        g_manualSpinVelocity = {};
        return;
    }

    if (IsMouseRotationActive()) {
        g_wasMouseRotationActive = true;
        g_resumeDelayRemaining = settings->resumeDelay;
        g_manualSpinVelocity = {};
        return;
    }

    if (g_wasMouseRotationActive) {
        g_wasMouseRotationActive = false;
        StartManualSpin(*settings);
    }

    if (ApplyManualSpin(*manager, *settings, frameDelta)) {
        return;
    }

    if (g_resumeDelayRemaining > 0.0F) {
        g_resumeDelayRemaining = std::max(0.0F, g_resumeDelayRemaining - frameDelta);
        return;
    }

    const float rotationSpeed = settings->rotationSpeed;
    if (rotationSpeed == 0.0F) {
        return;
    }

    RE::NiPoint2 rotationDelta {rotationSpeed * frameDelta, 0.0F};
    ApplyRotation(*manager, rotationDelta);
}

struct InventoryMenu_PostDisplay {
    static void thunk(RE::InventoryMenu* a_menu) {
        func(a_menu);
        ApplyAutoSpin();
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx {0x6};
};

struct Inventory3DManager_ProcessMouseMove {
    static bool thunk(RE::Inventory3DManager* a_manager, RE::MouseMoveEvent* a_event) {
        const bool processed = func(a_manager, a_event);
        if (processed && a_event && IsMouseRotationActive()) {
            CaptureManualVelocity(*a_event);
        }

        return processed;
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx {0x4};
};
}

void AutoSpin::Install() {
#ifndef __clang_analyzer__
    stl::write_vfunc<RE::InventoryMenu, InventoryMenu_PostDisplay>();
    stl::write_vfunc<Inventory3DManager_ProcessMouseMove>(RE::VTABLE_Inventory3DManager[0]);
#endif
    logger::info(
        "AutoSpin hook installed | menu=InventoryMenu | slot={} | input=Inventory3DManager | inputSlot={} | version={}",
        InventoryMenu_PostDisplay::idx,
        Inventory3DManager_ProcessMouseMove::idx,
        REL::Module::get().version()
    );
}
