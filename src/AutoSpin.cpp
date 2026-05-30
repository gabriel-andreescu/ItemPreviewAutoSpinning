#include "AutoSpin.h"

#include "Settings.h"

#include "RE/B/BSTimer.h"
#include "RE/I/INISettingCollection.h"
#include "RE/I/Inventory3DManager.h"
#include "RE/M/MouseMoveEvent.h"
#include "RE/N/NiPoint2.h"
#include "RE/Offsets_VTABLE.h"
#include "RE/S/Setting.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <xbyak/xbyak.h>

namespace {
inline constexpr float kMaxFrameDelta {1.0F / 20.0F};
inline constexpr float kManualVelocitySmoothing {0.45F};
inline constexpr float kManualSpinReleaseWindow {0.12F};
inline constexpr float kManualSpinMinFlickSpeed {0.45F};
inline constexpr float kManualSpinStopRatio {0.05F};
inline constexpr float kManualSpinStopSpeed {0.02F};
inline constexpr float kManualSpinMaxSpeed {8.0F};
inline constexpr float kManualSpinStaleAge {kManualSpinReleaseWindow + kMaxFrameDelta};
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

using ApplyInventoryPreviewRotation_t = void (*)(RE::Inventory3DManager*, RE::NiPoint2*);

float g_resumeDelayRemaining {0.0F};
RE::NiPoint2 g_dragVelocity {};
RE::NiPoint2 g_manualSpinVelocity {};
float g_timeSinceManualMove {kManualSpinStaleAge};
bool g_hasDragVelocity {false};
bool g_wasMouseRotationActive {false};

[[nodiscard]] bool HasExpectedInventory3DManagerRenderPrologue(const std::byte* a_address) noexcept {
    return std::memcmp(a_address, kInventory3DManagerRenderPrologue.data(), kInventory3DManagerRenderPrologue.size())
           == 0;
}

template <class T, std::size_t BYTES>
void HookFunctionPrologue(const std::uintptr_t a_src, const std::byte* a_originalBytes) {
    struct Patch : Xbyak::CodeGenerator {
        Patch(
            const std::uintptr_t a_originalFuncAddr,
            const std::byte* a_originalBytes,
            const std::size_t a_originalByteLength
        ) {
            for (::std::size_t i = 0; i < a_originalByteLength; ++i) {
                db(::std::to_integer<::std::uint8_t>(a_originalBytes[i]));
            }

            jmp(ptr[rip]);
            dq(a_originalFuncAddr + a_originalByteLength);
        }
    };

    Patch patch(a_src, a_originalBytes, BYTES);
    patch.ready();

    auto& trampoline = SKSE::GetTrampoline();
    trampoline.write_branch<BYTES>(a_src, T::thunk);

    const auto alloc = trampoline.allocate(patch.getSize());
    std::memcpy(alloc, patch.getCode(), patch.getSize());

    T::func = reinterpret_cast<std::uintptr_t>(alloc);
}

void LogUnsupportedInventory3DManagerRenderPrologue(const std::uintptr_t a_address, const std::byte* a_bytes) {
    logger::critical(
        "Hooks: Inventory3DManager::Render hook skipped | reason=unsupportedPrologue | address={:X} | bytes={:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
        a_address,
        std::to_integer<unsigned>(a_bytes[0]),
        std::to_integer<unsigned>(a_bytes[1]),
        std::to_integer<unsigned>(a_bytes[2]),
        std::to_integer<unsigned>(a_bytes[3]),
        std::to_integer<unsigned>(a_bytes[4]),
        std::to_integer<unsigned>(a_bytes[5])
    );
}

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

void ClearDragVelocity() {
    g_dragVelocity = {};
    g_timeSinceManualMove = kManualSpinStaleAge;
    g_hasDragVelocity = false;
}

void ResetPreviewState() {
    g_wasMouseRotationActive = false;
    ClearDragVelocity();
    g_manualSpinVelocity = {};
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

    if (GetSpeed(velocity) < kManualSpinMinFlickSpeed) {
        ClearDragVelocity();
        return;
    }

    const bool continueFlick = g_hasDragVelocity && g_timeSinceManualMove <= kManualSpinReleaseWindow;
    g_timeSinceManualMove = 0.0F;
    if (continueFlick) {
        g_dragVelocity.x = std::lerp(g_dragVelocity.x, velocity.x, kManualVelocitySmoothing);
        g_dragVelocity.y = std::lerp(g_dragVelocity.y, velocity.y, kManualVelocitySmoothing);
    } else {
        g_dragVelocity = velocity;
        g_hasDragVelocity = true;
    }
}

void StartManualSpin(const Settings& a_settings) {
    const bool canSpin = a_settings.manualSpinAfterDrag && a_settings.manualSpinStrength > 0.0F;
    const bool hasRecentFlick = g_hasDragVelocity && g_timeSinceManualMove <= kManualSpinReleaseWindow;
    if (!canSpin || !hasRecentFlick) {
        ClearDragVelocity();
        return;
    }

    g_manualSpinVelocity.x = g_dragVelocity.x * a_settings.manualSpinStrength;
    g_manualSpinVelocity.y = g_dragVelocity.y * a_settings.manualSpinStrength;
    ClampVelocity(g_manualSpinVelocity, kManualSpinMaxSpeed);

    ClearDragVelocity();

    if (GetSpeed(g_manualSpinVelocity) < kManualSpinStopSpeed) {
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

void ApplyAutoSpin(RE::Inventory3DManager& a_manager) {
    const float frameDelta = GetRealTimeDelta();
    if (frameDelta <= 0.0F) {
        return;
    }

    const auto* settings = Settings::GetSingleton();

    if (IsMouseRotationActive()) {
        g_wasMouseRotationActive = true;
        g_resumeDelayRemaining = settings->resumeDelay;
        g_manualSpinVelocity = {};
        g_timeSinceManualMove += frameDelta;
        return;
    }

    if (g_wasMouseRotationActive) {
        g_wasMouseRotationActive = false;
        StartManualSpin(*settings);
    }

    if (ApplyManualSpin(a_manager, *settings, frameDelta)) {
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
    ApplyRotation(a_manager, rotationDelta);
}

struct Inventory3DManager_Render {
    [[nodiscard]] static bool Install() {
        // SE:  Inventory3DManager::Render_140887750
        // AE:  Inventory3DManager::Render_140927560
        // GOG: Inventory3DManager::Render_1409295A0
        // VR:  Inventory3DManager::Render_1408B4C90
        REL::Relocation<std::byte*> target {RELOCATION_ID(50882, 51755)};
        const auto* targetBytes = target.get();
        const auto address = target.address();
        auto& trampoline = SKSE::GetTrampoline();

        if (REL::make_pattern<"E9">().match(address)) {
            func = trampoline.write_branch<kExistingBranchPatchSize>(address, thunk);

            logger::warn("Hooks: Inventory3DManager::Render hook chained | reason=existingBranch | branch=E9");
            return true;
        }

        if (HasExpectedInventory3DManagerRenderPrologue(targetBytes)) {
            HookFunctionPrologue<Inventory3DManager_Render, kInventory3DManagerRenderPatchSize>(address, targetBytes);

            logger::info("Hooks: Inventory3DManager::Render hook installed");
            return true;
        }

        LogUnsupportedInventory3DManagerRenderPrologue(address, targetBytes);
        return false;
    }

    static void thunk(RE::Inventory3DManager* a_manager) {
        if (!a_manager) {
            ResetPreviewState();
            return;
        }

        ApplyAutoSpin(*a_manager);
        func(a_manager);
    }

    static inline REL::Relocation<decltype(thunk)> func;
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
    if (!Inventory3DManager_Render::Install()) {
        stl::report_and_fail("Failed to install Inventory3DManager::Render hook"sv);
    }

    stl::write_vfunc<Inventory3DManager_ProcessMouseMove>(RE::VTABLE_Inventory3DManager[0]);
#endif
    logger::info(
        "AutoSpin hook installed | render=Inventory3DManager::Render | renderID=50882/51755 | input=Inventory3DManager | inputSlot={} | version={}",
        Inventory3DManager_ProcessMouseMove::idx,
        REL::Module::get().version()
    );
}
