#include "DxvUI/Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace DxvUI {

std::shared_ptr<spdlog::logger> Log::s_logger;

void Log::init() {
    // spdlog's registry throws if a logger with the same name already exists, so
    // a second init() must be a no-op instead of a crash.
    if (s_logger) return;

    // Set pattern for logs: [Timestamp] [Logger Name] [Log Level] Message
    spdlog::set_pattern("%^[%T] %n: %v%$");

    s_logger = spdlog::stdout_color_mt("DxvUI");

#ifdef NDEBUG
    // Release mode: only log info, warnings, and errors
    s_logger->set_level(spdlog::level::info);
#else
    // Debug mode: log everything
    s_logger->set_level(spdlog::level::trace);
#endif
}

void Log::setLevel(spdlog::level::level_enum level) {
    if (s_logger) s_logger->set_level(level);
}

}  // namespace DxvUI
