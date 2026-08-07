#ifndef DXVUI_LOG_H
#define DXVUI_LOG_H

#include <memory>

#include "spdlog/spdlog.h"

namespace DxvUI {

class Log {
   public:
    static void init();

    static void setLevel(spdlog::level::level_enum level);

    template <typename... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->trace(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->error(fmt, std::forward<Args>(args)...);
    }

   private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

}  // namespace DxvUI

#endif  // DXVUI_LOG_H
