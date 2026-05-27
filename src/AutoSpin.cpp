#include "AutoSpin.h"

#include "Settings.h"

#include "RE/B/BSTimer.h"
#include "RE/I/Inventory3DManager.h"
#include "RE/I/InventoryMenu.h"
#include "RE/N/NiPoint2.h"

#include <algorithm>

namespace {
inline constexpr float kMaxFrameDelta {1.0F / 20.0F};

using ApplyInventoryPreviewRotation_t = void (*)(RE::Inventory3DManager*, RE::NiPoint2*);

float g_resumeDelayRemaining {0.0F};

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

void ApplyAutoSpin() {
    const float frameDelta = GetRealTimeDelta();
    if (frameDelta <= 0.0F) {
        return;
    }

    const auto* settings = Settings::GetSingleton();

    if (IsMouseRotationActive()) {
        g_resumeDelayRemaining = settings->resumeDelay;
        return;
    }

    if (g_resumeDelayRemaining > 0.0F) {
        g_resumeDelayRemaining = std::max(0.0F, g_resumeDelayRemaining - frameDelta);
        return;
    }

    const float rotationSpeed = settings->rotationSpeed;
    if (rotationSpeed <= 0.0F) {
        return;
    }

    auto* manager = RE::Inventory3DManager::GetSingleton();
    if (!manager) {
        return;
    }

    // SE: 50902 -> 140888C20. AE: 51778 -> 140928C40. VR: 1408B65F0.
    static REL::Relocation<ApplyInventoryPreviewRotation_t> applyRotation {REL::VariantID(50902, 51778, 0x8B65F0)};

    RE::NiPoint2 rotationDelta {-rotationSpeed * frameDelta, 0.0F};
    applyRotation(manager, &rotationDelta);
}

struct InventoryMenu_PostDisplay {
    static void thunk(RE::InventoryMenu* a_menu) {
        func(a_menu);
        ApplyAutoSpin();
    }

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx {0x6};
};
}

void AutoSpin::Install() {
#ifndef __clang_analyzer__
    stl::write_vfunc<RE::InventoryMenu, InventoryMenu_PostDisplay>();
#endif
    logger::info(
        "AutoSpin hook installed | menu=InventoryMenu | slot={} | version={}",
        InventoryMenu_PostDisplay::idx,
        REL::Module::get().version()
    );
}
