#pragma once

#include <REX/REX/Singleton.h>

struct SpinSettings {
    float rotationSpeed {0.30F};
    float resumeDelay {0.35F};
    bool manualSpinAfterDrag {true};
    float manualSpinDuration {1.20F};
    float manualSpinStrength {1.00F};
    bool debugLogging {false};
};

class Settings : public REX::Singleton<Settings> {
public:
    void Reload();
    [[nodiscard]] const SpinSettings& GetValues() const {
        return _values;
    }

private:
    SpinSettings _values;
};
