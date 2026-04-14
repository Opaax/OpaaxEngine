#include "OpaaxLog.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Opaax {
    SharedPtr<spdlog::logger> OpaaxLog::s_CoreLogger;
    SharedPtr<spdlog::logger> OpaaxLog::s_ClientLogger;

    void OpaaxLog::Init()
    {
        // Config Pattern
        // [timestamp] [logger_name] [level] message
        // Example : [2024-11-07 15:30:45.123] [OPAAX] [info] Application started
        spdlog::set_pattern("%^[%T] [%n] [%l] %v%$");

        // Colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);

        // Log to file (op engine)
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("OpaaxEngine.log", true);
        file_sink->set_level(spdlog::level::trace);

        // OP Logger
        std::vector<spdlog::sink_ptr> core_sinks{ console_sink, file_sink };
        s_CoreLogger = std::make_shared<spdlog::logger>("OPAAX_Engine", core_sinks.begin(), core_sinks.end());
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);
        spdlog::register_logger(s_CoreLogger);

        // APP Logger
        s_ClientLogger = std::make_shared<spdlog::logger>("APP", core_sinks.begin(), core_sinks.end());
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
        spdlog::register_logger(s_ClientLogger);

        OPAAX_CORE_INFO("Log system initialized!");
    }

    void OpaaxLog::Shutdown()
    {
        if (s_CoreLogger != nullptr)
        {
            s_CoreLogger->flush();
            s_CoreLogger.reset();
        }

        if (s_ClientLogger != nullptr)
        {
            s_ClientLogger->flush();
            s_ClientLogger.reset();
        }

        // NOTE: spdlog::shutdown() joins the async thread if one exists and
        //   flushes all remaining sinks. Safe to call even in header-only mode.
        spdlog::shutdown();
    }

    SharedPtr<spdlog::logger>& OpaaxLog::GetCoreLogger()
    {
        return s_CoreLogger;
    }

    SharedPtr<spdlog::logger>& OpaaxLog::GetClientLogger()
    {
        return s_ClientLogger;
    }
}