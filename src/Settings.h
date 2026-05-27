#pragma once

#include <REX/REX/Singleton.h>

#include <CLIBUtil/simpleINI.hpp>

#include <algorithm>
#include <format>

class Settings : public REX::Singleton<Settings> {
public:
    inline static constexpr float kDefaultRotationSpeed {0.30F};
    inline static constexpr float kDefaultResumeDelay {0.35F};

    void Load() {
        using namespace clib_util::ini;

        rotationSpeed = kDefaultRotationSpeed;
        resumeDelay = kDefaultResumeDelay;

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

        rotationSpeed = std::clamp(rotationSpeed, 0.0F, 10.0F);
        resumeDelay = std::clamp(resumeDelay, 0.0F, 10.0F);
        ini.SetDoubleValue("General", "fRotationSpeed", rotationSpeed);
        ini.SetDoubleValue("General", "fResumeDelaySeconds", resumeDelay);

        (void)ini.SaveFile(path.c_str());

        logger::info(
            "Settings: loaded | path={} | rotationSpeed={:.2f} | resumeDelay={:.2f}",
            path,
            rotationSpeed,
            resumeDelay
        );
    }

    float rotationSpeed {kDefaultRotationSpeed};
    float resumeDelay {kDefaultResumeDelay};
};
