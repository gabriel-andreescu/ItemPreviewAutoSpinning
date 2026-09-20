#include "AutoSpin.h"
#include "Papyrus.h"
#include "Settings.h"

#include <SKSE/SKSE.h>

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_extender) {
    SKSE::Init(
        a_extender,
        {
            .logPattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v",
            .trampoline = true,
            .trampolineSize = 64,
        }
    );
    Settings::GetSingleton()->Reload();
    Papyrus::Register();
    AutoSpin::Install();
    return true;
}
