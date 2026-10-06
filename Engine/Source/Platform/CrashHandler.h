#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

#include <atomic>
#include <string>

namespace Opaax
{
    struct CrashHandlerSettings
    {
        /** Where Opaax_<timestamp>.dmp (and a copy of the log) are written. Created if missing. */
        OpaaxString DumpDir;

        /** The log file, copied next to the dump (the next launch overwrites the original). */
        OpaaxString LogFile;

        /** A native message box naming the dump. */
        bool bShowDialog = true;
    };

    // =============================================================================
    // CrashHandler — engine-wide crash reporting (Get()); tests create their own instance.
    //   On a crash it writes a minidump, logs the stack, copies the log next to the dump,
    //   shows a dialog, then the process ends.
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
         * Writes the dump, stack and log copy for InExceptionPointers (EXCEPTION_POINTERS*), or for
         * the current thread when null. No dialog, no exit.
         * @return The dump's path; empty if it could not be written
         */
        OpaaxString WriteReport(void* InExceptionPointers);

        /** WriteReport, the dialog, and the value the OS filter returns. */
        long HandleCrash(void* InExceptionPointers);

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
        std::wstring         m_DumpDirWide;
        std::wstring         m_LogFileWide;

        void*            m_PreviousFilter   = nullptr;
        std::atomic_flag m_bHandling        = ATOMIC_FLAG_INIT;
        bool             m_bInstalled       = false;
        bool             m_bSymbolsReady    = false;
    };
}
