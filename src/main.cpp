#include "AutoSpin.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace {
void SetupLog() {
    std::shared_ptr<spdlog::sinks::sink> sink;
    if (IsDebuggerPresent()) {
        sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    } else {
        // ReSharper disable once CppLocalVariableMayBeConst
        auto path = SKSE::log::log_directory();
        if (!path) {
            stl::report_and_fail("Failed to find standard logging directory"sv);
        }
        const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
        *path /= std::format("{}.log", plugin->GetName());
        sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    }

    auto logger = std::make_shared<spdlog::logger>("Global", std::move(sink));
    logger->set_level(
#ifdef NDEBUG
        spdlog::level::info
#else
        spdlog::level::debug
#endif
    );
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
        AutoSpin::Install();
        logger::info("Item Preview Auto Spinning initialized");
    }
}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SetupLog();

    SKSE::Init(a_skse, false);

    const auto* msg = SKSE::GetMessagingInterface();
    if (!msg) {
        logger::critical("Failed to obtain Messaging Interface");
        return false;
    }

    msg->RegisterListener(MessageHandler);

    const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    logger::info("{} loaded", plugin->GetName());
    return true;
}
