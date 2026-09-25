#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/LogHistory.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

#include <spdlog/spdlog.h>

#include <deque>
#include <iterator>
#include <mutex>

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

    /** A compile-time name. Declare with OPAAX_LOG_CATEGORY; there is no registry (I14's shape). */
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
    // Logger — an I1 singleton (SG1–SG5). Get() is the one engine-wide instance; tests build their
    //   own. Until Init attaches sinks, lines are held (bounded) and replayed on Init, so Bootstrap's
    //   first lines are never lost.
    // =============================================================================
    class OPAAX_API Logger final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Out-of-line in the DLL and leaked (SG4/SG5). */
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
        /** Attaches the colour console and the file sink at InLogFile. A file that cannot be opened
         *  leaves the console alone and says so. */
        void Init(OpaaxStringView InLogFile);

        /** Attaches the sinks and replays the held lines into EACH of them. */
        void AttachSinks(const TDynArray<spdlog::sink_ptr>& InSinks);
        void AttachSink(const spdlog::sink_ptr& InSink);

        /** Flushes and detaches every sink; later lines are held again, as before Init. */
        void Shutdown();

        void Flush();

        /** "[Category] message" — the one line shape every sink receives. */
        void Log(ELogLevel InLevel, const LogCategory& InCategory, OpaaxStringView InMessage);

        /** What OPAAX_LOG calls. The format string is checked at compile time. */
        template<typename... TArgs>
        void Logf(ELogLevel InLevel, const LogCategory& InCategory,
                  spdlog::format_string_t<TArgs...> InFormat, TArgs&&... InArgs)
        {
            fmt::memory_buffer lLine;
            fmt::format_to(std::back_inserter(lLine), "[{}] ", InCategory.Name);
            const size_t lMessageStart = lLine.size();
            fmt::format_to(std::back_inserter(lLine), InFormat, std::forward<TArgs>(InArgs)...);
            WriteLine(InLevel, InCategory, std::string_view(lLine.data(), lLine.size()), lMessageStart);
        }

        /**
         * Keep the last InCapacity lines, structured, for a reader like the editor's Log panel. Off (0)
         * by default: a game never pays for it. The editor turns it on before Bootstrap, so the boot
         * lines are kept too.
         */
        void EnableHistory(Uint32 InCapacity);

        /** LogHistory::CopySince, under the lock. The only way a reader sees the history. */
        Uint64 CopyHistorySince(Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const;

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        bool   HasSinks() const;
        Uint32 GetPendingCount() const;

    private:
        /** InLine is "[Category] message"; the message starts at InMessageStart. */
        void WriteLine(ELogLevel InLevel, const LogCategory& InCategory, std::string_view InLine, size_t InMessageStart);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct PendingLine
        {
            spdlog::log_clock::time_point Time;
            spdlog::level::level_enum     Level;
            std::string                   Text;
        };

        // Guards the sink list and the pending queue: spdlog's logger does not guard sinks().
        mutable std::mutex              m_Mutex;
        std::shared_ptr<spdlog::logger> m_Logger;
        std::deque<PendingLine>         m_Pending;
        Uint32                          m_DroppedPending = 0;
        LogHistory                      m_History;
    };
}

// These do NOT swallow the semicolon — the call site supplies it, so a log statement
// behaves like any other and `if (x) OPAAX_LOG(...); else` compiles.
#define OPAAX_LOG(Category, Level, Format, ...) \
    ::Opaax::Logger::Get().Logf(::Opaax::ELogLevel::Level, Category, Format, ##__VA_ARGS__)

#define OPAAX_APP_LOG(Level, Format, ...)       OPAAX_LOG(::Opaax::LogOpaaxApplication, Level, Format, ##__VA_ARGS__)
#define OPAAX_ENGINE_LOG(Level, Format, ...)    OPAAX_LOG(::Opaax::LogOpaaxEngine, Level, Format, ##__VA_ARGS__)

#define OPAAX_LOG_CATEGORY(Category) inline constexpr ::Opaax::LogCategory Log##Category{ #Category }
