#include "Core/Log/Logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Opaax
{
    namespace
    {
        // [date time.ms] [name] [level] [Category] message
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

        /** InText as spdlog takes it, without a copy. */
        spdlog::string_view_t ToSpdText(const OpaaxStringView InText)
        {
            return spdlog::string_view_t(InText.Data(), InText.GetLength());
        }
    }

    Logger& Logger::Get()
    {
        // Never destroyed, so logging from a static destructor still works.
        static Logger* s_Instance = new Logger();
        return *s_Instance;
    }

    Logger::Logger()
        // Not registered with spdlog, so several Loggers (engine, tests) can coexist.
        : m_Logger(MakeShared<spdlog::logger>(LOGGER_NAME))
    {
        m_Logger->set_level(spdlog::level::trace);
        m_Logger->flush_on(spdlog::level::trace);
    }

    Logger::~Logger() = default;

    void Logger::Init(const OpaaxStringView InLogFile)
    {
        TDynArray<spdlog::sink_ptr> lSinks{ MakeShared<spdlog::sinks::stdout_color_sink_mt>() };
        OpaaxString                 lFileError;

        try
        {
            lSinks.push_back(MakeShared<spdlog::sinks::basic_file_sink_mt>(
                spdlog::filename_t(InLogFile.Data(), InLogFile.GetLength()), true));
        }
        catch (const spdlog::spdlog_ex& lError)
        {
            lFileError = OpaaxString(lError.what());
        }

        for (const spdlog::sink_ptr& lSink : lSinks)
        {
            lSink->set_pattern(LINE_PATTERN);
        }

        // Replay held lines to the file as well as the console.
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
        TLockGuard<Mutex> lLock(m_Mutex);

        for (const spdlog::sink_ptr& lSink : InSinks)
        {
            lSink->set_level(spdlog::level::trace);

            for (const PendingLine& lLine : m_Pending)
            {
                lSink->log(spdlog::details::log_msg(lLine.Time, spdlog::source_loc{}, LOGGER_NAME, lLine.Level,
                                                    ToSpdText(lLine.Text)));
            }

            if (m_DroppedPending > 0)
            {
                const OpaaxString lNote = OpaaxString::Format("[{}] {} line(s) logged before the sinks existed were dropped",
                                                              LogOpaaxApplication.Name, m_DroppedPending);
                lSink->log(spdlog::details::log_msg(spdlog::source_loc{}, LOGGER_NAME, spdlog::level::warn, ToSpdText(lNote)));
            }

            lSink->flush();
            m_Logger->sinks().push_back(lSink);
        }

        m_Pending.clear();
        m_DroppedPending = 0;
    }

    void Logger::Shutdown()
    {
        TLockGuard<Mutex> lLock(m_Mutex);

        m_Logger->flush();
        m_Logger->sinks().clear();
    }

    void Logger::Flush()
    {
        TLockGuard<Mutex> lLock(m_Mutex);
        m_Logger->flush();
    }

    void Logger::Log(const ELogLevel InLevel, const LogCategory& InCategory, const OpaaxStringView InMessage)
    {
        Logf(InLevel, InCategory, "{}", InMessage);
    }

    void Logger::EnableHistory(const Uint32 InCapacity)
    {
        TLockGuard<Mutex> lLock(m_Mutex);
        m_History.SetCapacity(InCapacity);
    }

    Uint64 Logger::CopyHistorySince(const Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const
    {
        TLockGuard<Mutex> lLock(m_Mutex);
        return m_History.CopySince(InAfterSequence, OutEntries);
    }

    bool Logger::HasSinks() const
    {
        TLockGuard<Mutex> lLock(m_Mutex);
        return !m_Logger->sinks().empty();
    }

    Uint32 Logger::GetPendingCount() const
    {
        TLockGuard<Mutex> lLock(m_Mutex);
        return static_cast<Uint32>(m_Pending.size());
    }

    void Logger::WriteLine(const ELogLevel InLevel, const LogCategory& InCategory, const OpaaxStringView InLine,
                           const Uint32 InMessageStart)
    {
        TLockGuard<Mutex> lLock(m_Mutex);

        // Recorded in the history even before Init.
        if (m_History.IsEnabled())
        {
            m_History.Push(InLevel, OpaaxStringID(InCategory.Name), InLine.SubString(InMessageStart));
        }

        const spdlog::level::level_enum lLevel = ToSpdLevel(InLevel);

        if (!m_Logger->sinks().empty())
        {
            m_Logger->log(lLevel, ToSpdText(InLine));
            return;
        }

        // No sink yet: hold the line (keep the first ones).
        if (m_Pending.size() >= MAX_PENDING_LINES)
        {
            ++m_DroppedPending;
            return;
        }

        m_Pending.push_back({ spdlog::log_clock::now(), lLevel, InLine.ToString() });
    }
}
