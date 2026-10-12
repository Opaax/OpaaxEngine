#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/LogHistory.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

#include <spdlog/spdlog.h>

namespace Opaax
{
    enum class ELogLevel : Uint8
    {
        Trace,
        Info,
        Warn,
        Error,
        Critical
    };

    /** A log category name. Declare with OPAAX_LOG_CATEGORY. */
    struct LogCategory
    {
        constexpr explicit LogCategory(const char* InName)
            : Name(InName)
        {
        }

        const char* Name;
    };

    inline constexpr LogCategory LogOpaaxApplication{"OpaaxApplication"};
    inline constexpr LogCategory LogOpaaxEngine     {"OpaaxEngine"};

    // =============================================================================
    // Logger — the engine-wide logger (Get()); tests create their own.
    //   Lines logged before Init are held and replayed on Init.
    // =============================================================================
    class Logger final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Never destroyed. */
        static Logger& Get();

        static constexpr Uint32 MAX_PENDING_LINES = 256;

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        Logger();
        ~Logger();

        Logger(const Logger&)            = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&)                 = delete;
        Logger& operator=(Logger&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Adds the colour console and the file sink at InLogFile. */
        void Init(OpaaxStringView InLogFile);

        /** Adds the sinks and replays the held lines into them. */
        void AttachSinks(const TDynArray<spdlog::sink_ptr>& InSinks);
        void AttachSink(const spdlog::sink_ptr& InSink);

        /** Flushes and removes every sink; later lines are held again. */
        void Shutdown();

        void Flush();

        /** Formats "[Category] message". */
        void Log(ELogLevel InLevel, const LogCategory& InCategory, OpaaxStringView InMessage);

        /** Called by OPAAX_LOG. The format string is checked at compile time. */
        template<typename... TArgs>
        void Logf(ELogLevel InLevel, const LogCategory& InCategory,
                  spdlog::format_string_t<TArgs...> InFormat, TArgs&&... InArgs)
        {
            fmt::memory_buffer lLine;
            fmt::format_to(fmt::appender(lLine), "[{}] ", InCategory.Name);
            const Uint32 lMessageStart = static_cast<Uint32>(lLine.size());
            fmt::format_to(fmt::appender(lLine), InFormat, Forward<TArgs>(InArgs)...);
            WriteLine(InLevel, InCategory, OpaaxStringView(lLine.data(), static_cast<Uint32>(lLine.size())), lMessageStart);
        }

        /**
         * Keeps the last InCapacity lines for the editor's Log panel. Off (0) by default.
         */
        void EnableHistory(Uint32 InCapacity);

        /** Copies the history lines newer than InAfterSequence. */
        Uint64 CopyHistorySince(Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const;

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        bool   HasSinks() const;
        Uint32 GetPendingCount() const;

    private:
        /** InLine is "[Category] message"; the message starts at InMessageStart. */
        void WriteLine(ELogLevel InLevel, const LogCategory& InCategory, OpaaxStringView InLine, Uint32 InMessageStart);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct PendingLine
        {
            spdlog::log_clock::time_point Time;
            spdlog::level::level_enum     Level;
            OpaaxString                   Text;
        };

        // Guards the sinks and the pending lines.
        mutable Mutex              m_Mutex;
        TSharedPtr<spdlog::logger> m_Logger;
        TDeque<PendingLine>        m_Pending;
        Uint32                     m_DroppedPending = 0;
        LogHistory                 m_History;
    };
}

// The call site supplies the semicolon, like a normal statement.
#define OPAAX_LOG(Category, Level, Format, ...) \
    ::Opaax::Logger::Get().Logf(::Opaax::ELogLevel::Level, Category, Format, ##__VA_ARGS__)

#define OPAAX_APP_LOG(Level, Format, ...)       OPAAX_LOG(::Opaax::LogOpaaxApplication, Level, Format, ##__VA_ARGS__)
#define OPAAX_ENGINE_LOG(Level, Format, ...)    OPAAX_LOG(::Opaax::LogOpaaxEngine, Level, Format, ##__VA_ARGS__)

#define OPAAX_LOG_CATEGORY(Category) inline constexpr ::Opaax::LogCategory Log##Category{ #Category }
