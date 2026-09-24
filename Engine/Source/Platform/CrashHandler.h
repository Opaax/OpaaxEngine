#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

#include <atomic>
#include <string>

namespace Opaax
{
    struct CrashHandlerSettings
    {
        /** Where Opaax_<timestamp>.dmp (and a copy of the log) land. Created if missing. */
        OpaaxString DumpDir;

        /** The live log file, copied beside the dump — the next launch truncates the original. */
        OpaaxString LogFile;

        /** A native box naming the dump, so a crash in a shipped game is never silent. */
        bool bShowDialog = true;
    };

    // =============================================================================
    // CrashHandler — an I1 singleton (SG1–SG5): the OS keeps ONE unhandled-exception filter per
    //   process and its callback takes no user pointer, so the state it reads is process-wide by
    //   nature. Get() is what the hooks reach; a test builds its own instance and calls WriteReport.
    //
    //   A crash writes, most robust first: the minidump, the symbolized stack into the log, a copy
    //   of the log beside the dump; then the dialog; then the process ends.
    // =============================================================================
    class OPAAX_API CrashHandler final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Out-of-line in the DLL and leaked (SG4/SG5). */
        static CrashHandler& Get();

        /** Dev builds' `--crash-test`: a null write, reported like any real crash. */
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
        /** Settings only — what WriteReport uses. Paths are converted here, not at crash time. */
        void Configure(const CrashHandlerSettings& InSettings);

        /** Configure + the process-wide hooks. Call on the main thread (its stack reserve is set). */
        void Install(const CrashHandlerSettings& InSettings);
        void Uninstall();

        /**
         * Dump, stack and log copy for InExceptionPointers (an EXCEPTION_POINTERS*), or for the
         * calling thread right now when null. No dialog, no exit.
         *
         * @return The dump's path; empty when it could not be written.
         */
        OpaaxString WriteReport(void* InExceptionPointers);

        /** WriteReport, the dialog, and the answer the OS filter returns. */
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
