#include "SpinState.h"
#include "PCH.h" // IWYU pragma: keep
#include "Settings.h"
#include <RE/N/NiPoint2.h>
#include <algorithm>
#include <cmath>
namespace {
inline constexpr float kMaxFrameDelta {1.0F / 20.0F};
inline constexpr float kManualVelocitySmoothing {0.45F};
inline constexpr float kManualSpinReleaseWindow {0.12F};
inline constexpr float kManualSpinMinFlickSpeed {0.45F};
inline constexpr float kManualSpinStopRatio {0.05F};
inline constexpr float kManualSpinStopSpeed {0.02F};
inline constexpr float kManualSpinMaxSpeed {8.0F};
inline constexpr float kManualSpinStaleAge {kManualSpinReleaseWindow + kMaxFrameDelta};
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
}

void SpinState::ClearDragVelocity() {
    _dragVelocity = {};
    _timeSinceManualMove = kManualSpinStaleAge;
    _hasDragVelocity = false;
}

void SpinState::Reset() {
    _resumeDelayRemaining = 0.0F;
    _wasMouseRotationActive = false;
    ClearDragVelocity();
    _manualSpinVelocity = {};
}

void SpinState::CaptureVelocity(RE::NiPoint2 a_velocity) {
    if (GetSpeed(a_velocity) < kManualSpinMinFlickSpeed) {
        ClearDragVelocity();
        return;
    }

    const bool continueFlick = _hasDragVelocity && _timeSinceManualMove <= kManualSpinReleaseWindow;
    _timeSinceManualMove = 0.0F;
    if (continueFlick) {
        _dragVelocity.x = std::lerp(_dragVelocity.x, a_velocity.x, kManualVelocitySmoothing);
        _dragVelocity.y = std::lerp(_dragVelocity.y, a_velocity.y, kManualVelocitySmoothing);
    } else {
        _dragVelocity = a_velocity;
        _hasDragVelocity = true;
    }
}

void SpinState::StartManualSpin(const SpinSettings& a_settings) {
    const bool canSpin = a_settings.manualSpinAfterDrag && a_settings.manualSpinStrength > 0.0F;
    const bool hasRecentFlick = _hasDragVelocity && _timeSinceManualMove <= kManualSpinReleaseWindow;
    if (!canSpin || !hasRecentFlick) {
        ClearDragVelocity();
        return;
    }

    _manualSpinVelocity.x = _dragVelocity.x * a_settings.manualSpinStrength;
    _manualSpinVelocity.y = _dragVelocity.y * a_settings.manualSpinStrength;
    ClampVelocity(_manualSpinVelocity, kManualSpinMaxSpeed);

    ClearDragVelocity();

    if (GetSpeed(_manualSpinVelocity) < kManualSpinStopSpeed) {
        _manualSpinVelocity = {};
    }
}

RE::NiPoint2 SpinState::ApplyManualSpin(const SpinSettings& a_settings, const float a_frameDelta) {
    RE::NiPoint2 rotationDelta {_manualSpinVelocity.x * a_frameDelta, _manualSpinVelocity.y * a_frameDelta};

    const float decay = std::pow(kManualSpinStopRatio, a_frameDelta / a_settings.manualSpinDuration);
    _manualSpinVelocity.x *= decay;
    _manualSpinVelocity.y *= decay;

    return rotationDelta;
}

RE::NiPoint2 SpinState::Update(const SpinSettings& a_settings, bool a_mouseRotationActive, float a_frameDelta) {
    a_frameDelta = std::clamp(a_frameDelta, 0.0F, kMaxFrameDelta);
    if (a_frameDelta <= 0.0F) {
        return {};
    }
    if (a_mouseRotationActive) {
        _wasMouseRotationActive = true;
        _resumeDelayRemaining = a_settings.resumeDelay;
        _manualSpinVelocity = {};
        _timeSinceManualMove += a_frameDelta;
        return {};
    }
    if (_wasMouseRotationActive) {
        _wasMouseRotationActive = false;
        StartManualSpin(a_settings);
    }
    if (GetSpeed(_manualSpinVelocity) >= kManualSpinStopSpeed) {
        return ApplyManualSpin(a_settings, a_frameDelta);
    }
    _manualSpinVelocity = {};
    if (_resumeDelayRemaining > 0.0F) {
        _resumeDelayRemaining = std::max(0.0F, _resumeDelayRemaining - a_frameDelta);
        return {};
    }
    return {a_settings.rotationSpeed * a_frameDelta, 0.0F};
}
