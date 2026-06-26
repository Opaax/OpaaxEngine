#include "ILogger.h"

#include <iostream>

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    namespace
    {
        spdlog::level::level_enum ToSpdLevel(ELogLevel InLevel)
        {
            switch (InLevel)
            {
                case ELogLevel::Trace:    return spdlog::level::trace;
                case ELogLevel::Info:     return spdlog::level::info;
                case ELogLevel::Warn:     return spdlog::level::warn;
                case ELogLevel::Error:    return spdlog::level::err;
                case ELogLevel::Critical: return spdlog::level::critical;
            }
            return spdlog::level::info;
        }

        // =====================================================================
        // NullLogger — drops every message.
        // =====================================================================
        class NullLogger final : public ILogger
        {
        public:
            bool IsNull() const noexcept override { return true; }
            void Log(ELogLevel, const OpaaxString&) override {}
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

    void Logger::Log(ELogLevel InLevel, const OpaaxString& InMessage)
    {
        // "{}" + the message as an ARG — user text must never be parsed as a fmt string.
        if (const auto& lCore = OpaaxLog::GetCoreLogger())
        {
            lCore->log(ToSpdLevel(InLevel), "{}", InMessage.CStr());
        }
    }
}
