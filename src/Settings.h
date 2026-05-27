#pragma once

#include <REX/REX/Singleton.h>

#include <CLIBUtil/simpleINI.hpp>

#include <algorithm>
#include <format>

class Settings : public REX::Singleton<Settings> {
public:
    inline static constexpr float kDefaultRotationSpeed {0.30F};
    inline static constexpr float kDefaultResumeDelay {0.35F};
    inline static constexpr bool kDefaultManualSpinAfterDrag {true};
    inline static constexpr float kDefaultManualSpinDuration {1.20F};
    inline static constexpr float kDefaultManualSpinStrength {1.00F};

    void Load() {
        using namespace clib_util::ini;

        rotationSpeed = kDefaultRotationSpeed;
        resumeDelay = kDefaultResumeDelay;
        manualSpinAfterDrag = kDefaultManualSpinAfterDrag;
        manualSpinDuration = kDefaultManualSpinDuration;
        manualSpinStrength = kDefaultManualSpinStrength;

        const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
        const auto path = std::format("Data/SKSE/Plugins/{}.ini", plugin->GetName());

        CSimpleIniA ini;
        ini.SetUnicode();

        (void)ini.LoadFile(path.c_str());
        get_value(
            ini,
            rotationSpeed,
            "General",
            "fRotationSpeed",
            "; Automatic rotation speed.\n; Valid range: 0-10.\n; Default: 0.3"
        );
        get_value(
            ini,
            resumeDelay,
            "General",
            "fResumeDelaySeconds",
            "; Resume delay after manual rotation.\n; Valid range: 0-10.\n; Default: 0.35"
        );
        get_value(
            ini,
            manualSpinAfterDrag,
            "General",
            "bManualSpinAfterDrag",
            "; Continue spinning briefly after manual drag.\n; Default: true"
        );
        get_value(
            ini,
            manualSpinDuration,
            "General",
            "fManualSpinDurationSeconds",
            "; How long the release spin takes to settle.\n; Valid range: 0.1-5.\n; Default: 1.2"
        );
        get_value(
            ini,
            manualSpinStrength,
            "General",
            "fManualSpinStrength",
            "; Release spin strength multiplier.\n; Valid range: 0-3.\n; Default: 1"
        );

        rotationSpeed = std::clamp(rotationSpeed, 0.0F, 10.0F);
        resumeDelay = std::clamp(resumeDelay, 0.0F, 10.0F);
        manualSpinDuration = std::clamp(manualSpinDuration, 0.1F, 5.0F);
        manualSpinStrength = std::clamp(manualSpinStrength, 0.0F, 3.0F);
        ini.SetDoubleValue("General", "fRotationSpeed", rotationSpeed);
        ini.SetDoubleValue("General", "fResumeDelaySeconds", resumeDelay);
        ini.SetBoolValue("General", "bManualSpinAfterDrag", manualSpinAfterDrag);
        ini.SetDoubleValue("General", "fManualSpinDurationSeconds", manualSpinDuration);
        ini.SetDoubleValue("General", "fManualSpinStrength", manualSpinStrength);

        (void)ini.SaveFile(path.c_str());

        logger::info(
            "Settings: loaded | path={} | rotationSpeed={:.2f} | resumeDelay={:.2f} | manualSpin={} | manualSpinDuration={:.2f} | manualSpinStrength={:.2f}",
            path,
            rotationSpeed,
            resumeDelay,
            manualSpinAfterDrag,
            manualSpinDuration,
            manualSpinStrength
        );
    }

    float rotationSpeed {kDefaultRotationSpeed};
    float resumeDelay {kDefaultResumeDelay};
    bool manualSpinAfterDrag {kDefaultManualSpinAfterDrag};
    float manualSpinDuration {kDefaultManualSpinDuration};
    float manualSpinStrength {kDefaultManualSpinStrength};
};
