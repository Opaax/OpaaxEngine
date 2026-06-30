#include "ILogger.h"

#include <iostream>

#include "Core/Application/OpaaxApplication.h"
#include "Core/Log/OpaaxLog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/null_sink.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullLogger — drops every message.
        // =====================================================================
        class NullLogger final : public ILogger
        {
        public:
            // The OPAAX_LOG macro dereferences AppLogger directly (bypassing the null-guarded
            // virtual Log()), so even the null object MUST hold a valid logger — a null sink that
            // silently discards every message. Keeps "degrade, don't crash" when no logger is provided.
            NullLogger()
            {
                AppLogger = std::make_shared<spdlog::logger>(
                    "OPAAX_Null", std::make_shared<spdlog::sinks::null_sink_mt>());
            }

            bool IsNull() const noexcept override { return true; }
            void Log(ELogLevel, const OpaaxString&) override {}
            void Log(ELogLevel, const char* Category, const OpaaxString&) override {}
        };
    }

    ServiceTypeID ILogger::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    ILogger& ILogger::Null()
    {
        static NullLogger s_Null;
        return s_Null;
    }

    ILogger& GetLogger()
    {
        return OpaaxApplication::GetAppService<ILogger>();
    }

    Logger::Logger(Opaax::IPaths& InPaths)
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
        
        //Already exist with old OpaaxLog::Init()
        if ((AppLogger = spdlog::get("OPAAX_Engine")) != nullptr)
        {
            return;
        }

        // OP Logger
        std::vector<spdlog::sink_ptr> core_sinks{ console_sink, file_sink };
        AppLogger = std::make_shared<spdlog::logger>("OPAAX_Engine", core_sinks.begin(), core_sinks.end());
        AppLogger->set_level(spdlog::level::trace);
        AppLogger->flush_on(spdlog::level::trace);
        spdlog::register_logger(AppLogger);
    }

    void Logger::Log(ELogLevel InLevel, const OpaaxString& InMessage)
    {
        if (AppLogger != nullptr)
        {
            AppLogger->log(ToSpdLevel(InLevel), "{}", InMessage.CStr());
        }
    }

    void Logger::Log(ELogLevel InLevel, const char* Category, const OpaaxString& InMessage)
    {
        if (AppLogger != nullptr)
        {
            AppLogger->log(
            ToSpdLevel(InLevel),
            "[{}] {}",
            Category,
            InMessage.CStr());
        }
    }
}
