#pragma once

#include <cstddef> // IWYU pragma: keep

#include <RE/N/NiPoint2.h>

struct SpinSettings;
class SpinState {
public:
    void Reset();
    void CaptureVelocity(RE::NiPoint2 a_velocity);
    [[nodiscard]] RE::NiPoint2 Update(const SpinSettings& a_settings, bool a_mouseRotationActive, float a_frameDelta);

private:
    void ClearDragVelocity();
    void StartManualSpin(const SpinSettings& a_settings);
    [[nodiscard]] RE::NiPoint2 ApplyManualSpin(const SpinSettings& a_settings, float a_frameDelta);
    float _resumeDelayRemaining {};
    RE::NiPoint2 _dragVelocity;
    RE::NiPoint2 _manualSpinVelocity;
    float _timeSinceManualMove {};
    bool _hasDragVelocity {};
    bool _wasMouseRotationActive {};
};
