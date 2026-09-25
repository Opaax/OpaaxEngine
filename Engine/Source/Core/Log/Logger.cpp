#include "Logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Opaax
{
    namespace
    {
        // [date time.ms] [name] [level] [Category] message — spdlog's default, which is what every host
        // actually printed (the old global set_pattern ran before the logger existed, so never applied).
        constexpr const char* LINE_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";
        constexpr const char* LOGGER_NAME  = "OPAAX_Engine";

        spdlog::level::level_enum ToSpdLevel(const ELogLevel InLevel)
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
    }

    Logger& Logger::Get()
    {
        // Leaked on purpose (SG5): a line from a static destructor must still land somewhere legal.
        static Logger* s_Instance = new Logger();
        return *s_Instance;
    }

    Logger::Logger()
        // Never registered with spdlog's global registry: two Loggers (the engine's, a test's) must
        // not collide on a name.
        : m_Logger(std::make_shared<spdlog::logger>(LOGGER_NAME))
    {
        m_Logger->set_level(spdlog::level::trace);
        m_Logger->flush_on(spdlog::level::trace);
    }

    Logger::~Logger() = default;

    void Logger::Init(const OpaaxStringView InLogFile)
    {
        TDynArray<spdlog::sink_ptr> lSinks{ std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
        OpaaxString                 lFileError;

        try
        {
            lSinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                std::string(InLogFile.Data(), InLogFile.GetLength()), true));
        }
        catch (const spdlog::spdlog_ex& lError)
        {
            lFileError = OpaaxString(lError.what());
        }

        for (const spdlog::sink_ptr& lSink : lSinks)
        {
            lSink->set_pattern(LINE_PATTERN);
        }

        // Together, so the held lines reach the file as well as the console.
        AttachSinks(lSinks);

        if (!lFileError.IsEmpty())
        {
            Logf(ELogLevel::Error, LogOpaaxApplication, "Logger: cannot open '{}' ({}), console only",
                 InLogFile, lFileError);
        }
    }

    void Logger::AttachSink(const spdlog::sink_ptr& InSink)
    {
        AttachSinks({ InSink });
    }

    void Logger::AttachSinks(const TDynArray<spdlog::sink_ptr>& InSinks)
    {
        std::lock_guard lLock(m_Mutex);

        for (const spdlog::sink_ptr& lSink : InSinks)
        {
            lSink->set_level(spdlog::level::trace);

            for (const PendingLine& lLine : m_Pending)
            {
                lSink->log(spdlog::details::log_msg(lLine.Time, spdlog::source_loc{}, LOGGER_NAME, lLine.Level, lLine.Text));
            }

            if (m_DroppedPending > 0)
            {
                const std::string lNote = fmt::format("[{}] {} line(s) logged before the sinks existed were dropped",
                                                      LogOpaaxApplication.Name, m_DroppedPending);
                lSink->log(spdlog::details::log_msg(spdlog::source_loc{}, LOGGER_NAME, spdlog::level::warn, lNote));
            }

            lSink->flush();
            m_Logger->sinks().push_back(lSink);
        }

        m_Pending.clear();
        m_DroppedPending = 0;
    }

    void Logger::Shutdown()
    {
        std::lock_guard lLock(m_Mutex);

        m_Logger->flush();
        m_Logger->sinks().clear();
    }

    void Logger::Flush()
    {
        std::lock_guard lLock(m_Mutex);
        m_Logger->flush();
    }

    void Logger::Log(const ELogLevel InLevel, const LogCategory& InCategory, const OpaaxStringView InMessage)
    {
        Logf(InLevel, InCategory, "{}", InMessage);
    }

    bool Logger::HasSinks() const
    {
        std::lock_guard lLock(m_Mutex);
        return !m_Logger->sinks().empty();
    }

    Uint32 Logger::GetPendingCount() const
    {
        std::lock_guard lLock(m_Mutex);
        return static_cast<Uint32>(m_Pending.size());
    }

    void Logger::WriteLine(const ELogLevel InLevel, const std::string_view InLine)
    {
        std::lock_guard lLock(m_Mutex);

        const spdlog::level::level_enum lLevel = ToSpdLevel(InLevel);

        if (!m_Logger->sinks().empty())
        {
            m_Logger->log(lLevel, InLine);
            return;
        }

        // No sink yet (or any more): hold the line. Keep the FIRST ones — at boot they are the ones
        // that explain everything after.
        if (m_Pending.size() >= MAX_PENDING_LINES)
        {
            ++m_DroppedPending;
            return;
        }

        m_Pending.push_back({ spdlog::log_clock::now(), lLevel, std::string(InLine) });
    }
}
