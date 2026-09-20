#include "Settings.h"

#include <SKSE/SKSE.h>

#include <BMK/Settings.h>
#include <CLIBUtil/simpleINI.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace {
constexpr auto kSection = "General";

void ReadValues(CSimpleIniA& a_ini, SpinSettings& a_values) {
    clib_util::ini::get_value(
        a_ini,
        a_values.rotationSpeed,
        kSection,
        "fRotationSpeed",
        "; Automatic rotation speed. Negative values reverse direction. Range: -10 to 10."
    );
    clib_util::ini::get_value(
        a_ini,
        a_values.resumeDelay,
        kSection,
        "fResumeDelaySeconds",
        "; Resume delay after manual rotation. Range: 0 to 10."
    );
    clib_util::ini::get_value(
        a_ini,
        a_values.manualSpinAfterDrag,
        kSection,
        "bManualSpinAfterDrag",
        "; Continue spinning briefly after manual drag.",
        clib_util::ini::bool_format::kNumeric
    );
    clib_util::ini::get_value(
        a_ini,
        a_values.manualSpinDuration,
        kSection,
        "fManualSpinDurationSeconds",
        "; How long the release spin takes to settle. Range: 0.1 to 5."
    );
    clib_util::ini::get_value(
        a_ini,
        a_values.manualSpinStrength,
        kSection,
        "fManualSpinStrength",
        "; Release spin strength multiplier. Range: 0 to 3."
    );
    clib_util::ini::get_value(
        a_ini,
        a_values.debugLogging,
        kSection,
        "bDebugLogging",
        "; Enable debug logging.",
        clib_util::ini::bool_format::kNumeric
    );
}

// NOLINTNEXTLINE(readability-function-size)
float ValidateFloat(
    CSimpleIniA& a_user,
    const std::filesystem::path& a_userPath,
    const char* a_key,
    const float a_raw,
    const float a_fallback,
    const float a_minimum,
    const float a_maximum
) {
    const float value = std::isfinite(a_raw) ? std::clamp(a_raw, a_minimum, a_maximum) : a_fallback;
    if (!std::isfinite(a_raw) || value != a_raw) {
        SKSE::log::warn("{}: invalid {}={} replaced with {}", a_userPath.string(), a_key, a_raw, value);
    }
    if (a_user.SetDoubleValue(kSection, a_key, value) < 0) {
        SKSE::log::error("Cannot update {} in {}", a_key, a_userPath.string());
    }
    return value;
}

void ValidateValues(CSimpleIniA& a_user, const std::filesystem::path& a_userPath, SpinSettings& a_values) {
    const SpinSettings fallback;
    a_values.rotationSpeed = ValidateFloat(
        a_user,
        a_userPath,
        "fRotationSpeed",
        a_values.rotationSpeed,
        fallback.rotationSpeed,
        -10.0F,
        10.0F
    );
    a_values.resumeDelay = ValidateFloat(
        a_user,
        a_userPath,
        "fResumeDelaySeconds",
        a_values.resumeDelay,
        fallback.resumeDelay,
        0.0F,
        10.0F
    );
    a_values.manualSpinDuration = ValidateFloat(
        a_user,
        a_userPath,
        "fManualSpinDurationSeconds",
        a_values.manualSpinDuration,
        fallback.manualSpinDuration,
        0.1F,
        5.0F
    );
    a_values.manualSpinStrength = ValidateFloat(
        a_user,
        a_userPath,
        "fManualSpinStrength",
        a_values.manualSpinStrength,
        fallback.manualSpinStrength,
        0.0F,
        3.0F
    );
}
}

namespace {
std::optional<SpinSettings> ReadSettings(
    const std::filesystem::path& a_defaultPath,
    const std::filesystem::path& a_userPath
) {
    const auto initialValues = SpinSettings {};
    auto loaded = BMK::Settings::Load(
        {
            .defaults = a_defaultPath,
            .user = a_userPath,
        },
        initialValues,
        [&a_userPath](CSimpleIniA& a_defaults, CSimpleIniA& a_user, SpinSettings& a_candidate) {
            ReadValues(a_defaults, a_candidate);
            ReadValues(a_user, a_candidate);
            ValidateValues(a_user, a_userPath, a_candidate);
        }
    );
    if (!loaded) {
        SKSE::log::warn("Cannot load settings: {}", loaded.error().message);
        return std::nullopt;
    }
    if (loaded->saveFailure) {
        SKSE::log::warn("Cannot save settings: {}", loaded->saveFailure->message);
    }
    return loaded->values;
}
}

void Settings::Reload() {
    const auto values = ReadSettings(
        L"Data/MCM/Config/ItemPreviewAutoSpinning/settings.ini",
        L"Data/MCM/Settings/ItemPreviewAutoSpinning.ini"
    );
    if (!values) {
        return;
    }

    _values = *values;
    BMK::Settings::ApplyLogLevel(values->debugLogging, SKSE::InitInfo {}.logLevel);
    SKSE::log::info(
        "Settings: speed={}, resumeDelay={}, releaseSpin={}, duration={}, strength={}",
        values->rotationSpeed,
        values->resumeDelay,
        values->manualSpinAfterDrag,
        values->manualSpinDuration,
        values->manualSpinStrength
    );
}
