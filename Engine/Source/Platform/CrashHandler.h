#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

#include <atomic>
#include <filesystem>

namespace Opaax
{
    struct CrashHandlerSettings
    {
        /**
         * Where the report (Opaax_<timestamp>.dmp on Windows, .txt elsewhere) and a copy of the log
         * are written. Created if missing.
         */
        OpaaxString DumpDir;

        /** The log file, copied next to the report (the next launch overwrites the original). */
        OpaaxString LogFile;

        /** Tells the user where the report is: a message box on Windows, stderr elsewhere. */
        bool bShowDialog = true;
    };

    // =============================================================================
    // CrashHandler — engine-wide crash reporting (Get()); tests create their own instance.
    //   On a crash it writes a report (a minidump on Windows, a stack trace elsewhere), logs the
    //   stack, copies the log next to the report, tells the user, then the process ends.
    // =============================================================================
    class CrashHandler final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Never destroyed. */
        static CrashHandler& Get();

        /** Dev builds' --crash-test: a null write, reported like a real crash. */
        static void TriggerTestCrash();

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        CrashHandler();
        ~CrashHandler();

        CrashHandler(const CrashHandler&)            = delete;
        CrashHandler& operator=(const CrashHandler&) = delete;
        CrashHandler(CrashHandler&&)                 = delete;
        CrashHandler& operator=(CrashHandler&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Settings only (used by WriteReport). */
        void Configure(const CrashHandlerSettings& InSettings);

        /** Configures and installs the process-wide hooks. Call on the main thread. */
        void Install(const CrashHandlerSettings& InSettings);
        void Uninstall();

        /**
         * Writes the report, stack and log copy. No dialog, no exit.
         * @param InCrashContext The OS crash context (EXCEPTION_POINTERS* on Windows, a pointer to the
         *   signal number elsewhere), or null for a report on the calling thread
         * @return The report's path; empty if it could not be written
         */
        OpaaxString WriteReport(void* InCrashContext);

        /** WriteReport, then tells the user. Returns the value the Windows exception filter expects. */
        long HandleCrash(void* InCrashContext);

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        bool IsInstalled() const noexcept { return m_bInstalled; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CrashHandlerSettings m_Settings;

        // Native path strings (UTF-16 on Windows), prepared at Configure so a crash does not convert.
        std::filesystem::path::string_type m_DumpDirNative;
        std::filesystem::path::string_type m_LogFileNative;

        void*            m_PreviousFilter   = nullptr;
        std::atomic_flag m_bHandling        = ATOMIC_FLAG_INIT;
        bool             m_bInstalled       = false;
        bool             m_bSymbolsReady    = false;
    };
}
