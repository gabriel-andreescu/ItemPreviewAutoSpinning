#include "Papyrus.h"

#include "Settings.h"

#include <RE/Skyrim.h> // IWYU pragma: keep
#include <SKSE/SKSE.h>

namespace Papyrus {
namespace {
    // Papyrus supplies the native base tag as a mutable pointer.
    // NOLINTNEXTLINE(misc-const-correctness)
    void ReloadSettings([[maybe_unused]] RE::StaticFunctionTag* a_base) {
        SKSE::GetTaskInterface()->AddUITask([] { Settings::GetSingleton()->Reload(); });
    }

    bool RegisterMCM(RE::BSScript::IVirtualMachine* a_virtualMachine) {
        a_virtualMachine->RegisterFunction("ReloadSettings", "ItemPreviewAutoSpinning_Native", ReloadSettings);
        SKSE::log::info("Papyrus: MCM reload callback registered");
        return true;
    }
}

void Register() {
    const auto* papyrus = SKSE::GetPapyrusInterface();
    if (papyrus == nullptr) {
        SKSE::log::critical("Papyrus: register skipped | reason=noInterface");
        return;
    }

    papyrus->Register(RegisterMCM);
}
}
