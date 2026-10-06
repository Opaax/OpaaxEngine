#include "Platform/CrashHandler.h"

#ifdef OPAAX_PLATFORM_POSIX

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxUtf8.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <string>

#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(CrashHandler);

    namespace
    {
        constexpr int MAX_STACK_FRAMES = 64;

        constexpr int CRASH_SIGNALS[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
        constexpr int SIGNAL_COUNT    = static_cast<int>(sizeof(CRASH_SIGNALS) / sizeof(CRASH_SIGNALS[0]));

        // Passed to HandleCrash in place of a signal number when std::terminate is called.
        constexpr int TERMINATE_PSEUDO_SIGNAL = -1;

        struct sigaction g_PreviousActions[SIGNAL_COUNT];

        // The handler runs on its own stack, so a stack overflow can still be reported.
        constexpr size_t ALT_STACK_BYTES = 256 * 1024;
        alignas(16) char g_AltStack[ALT_STACK_BYTES];

        const char* DescribeSignal(const int InSignal)
        {
            switch (InSignal)
            {
            case SIGSEGV:                 return "segmentation fault";
            case SIGBUS:                  return "bus error";
            case SIGILL:                  return "illegal instruction";
            case SIGFPE:                  return "floating point exception";
            case SIGABRT:                 return "abort()";
            case TERMINATE_PSEUDO_SIGNAL: return "std::terminate (uncaught C++ exception?)";
            default:                      return "unknown signal";
            }
        }

        void OnCrashSignal(int InSignal, siginfo_t*, void*)
        {
            int lSignal = InSignal;
            CrashHandler::Get().HandleCrash(&lSignal);

            // The default action then ends the process (exit status, core dump).
            std::signal(InSignal, SIG_DFL);
            std::raise(InSignal);
        }

        void OnTerminate()
        {
            int lSignal = TERMINATE_PSEUDO_SIGNAL;
            CrashHandler::Get().HandleCrash(&lSignal);

            std::signal(SIGABRT, SIG_DFL);
            std::abort();
        }

        /** Opaax_yyyymmdd_hhmmss, without heap allocation (the heap may be corrupted). */
        void FormatStamp(char (&OutStamp)[32])
        {
            const std::time_t lNow = std::time(nullptr);
            std::tm           lLocal{};
            localtime_r(&lNow, &lLocal);

            std::snprintf(OutStamp, sizeof(OutStamp), "Opaax_%04d%02d%02d_%02d%02d%02d",
                          lLocal.tm_year + 1900, lLocal.tm_mon + 1, lLocal.tm_mday,
                          lLocal.tm_hour, lLocal.tm_min, lLocal.tm_sec);
        }

        void WriteText(const int InFd, const char* InText)
        {
            if (InFd >= 0)
            {
                (void)!write(InFd, InText, std::strlen(InText));
            }
        }
    }

    CrashHandler& CrashHandler::Get()
    {
        // Never destroyed: a crash during static destruction must still find it.
        static CrashHandler* s_Instance = new CrashHandler();
        return *s_Instance;
    }

    CrashHandler::CrashHandler() = default;

    CrashHandler::~CrashHandler()
    {
        // Only a test's instance gets here (Get()'s is never destroyed).
        Uninstall();
    }

    void CrashHandler::TriggerTestCrash()
    {
        OPAAX_LOG(LogCrashHandler, Warn, "--crash-test: writing through a null pointer on purpose");

        volatile int* lNull = nullptr;
        *lNull = 0x0DEAD;
    }

    void CrashHandler::Configure(const CrashHandlerSettings& InSettings)
    {
        m_Settings      = InSettings;
        m_DumpDirNative = Utf8::ToFsPath(InSettings.DumpDir).native();
        m_LogFileNative = Utf8::ToFsPath(InSettings.LogFile).native();

        std::error_code lError;
        std::filesystem::create_directories(std::filesystem::path(m_DumpDirNative), lError);
    }

    void CrashHandler::Install(const CrashHandlerSettings& InSettings)
    {
        Configure(InSettings);

        if (m_bInstalled) { return; }

        stack_t lStack{};
        lStack.ss_sp    = g_AltStack;
        lStack.ss_size  = sizeof(g_AltStack);
        lStack.ss_flags = 0;
        sigaltstack(&lStack, nullptr);

        struct sigaction lAction{};
        lAction.sa_sigaction = &OnCrashSignal;
        lAction.sa_flags     = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&lAction.sa_mask);

        for (int i = 0; i < SIGNAL_COUNT; ++i)
        {
            sigaction(CRASH_SIGNALS[i], &lAction, &g_PreviousActions[i]);
        }

        std::set_terminate(&OnTerminate);

        m_bInstalled = true;

        OPAAX_LOG(LogCrashHandler, Info, "Crash handler installed — reports to {}", m_Settings.DumpDir);
    }

    void CrashHandler::Uninstall()
    {
        if (!m_bInstalled) { return; }

        for (int i = 0; i < SIGNAL_COUNT; ++i)
        {
            sigaction(CRASH_SIGNALS[i], &g_PreviousActions[i], nullptr);
        }

        std::set_terminate(nullptr);

        m_bInstalled = false;
    }

    OpaaxString CrashHandler::WriteReport(void* InSignal)
    {
        const int lSignal = (InSignal != nullptr) ? *static_cast<const int*>(InSignal) : 0;

        char lStamp[32];
        FormatStamp(lStamp);

        const std::string lReportPath = m_DumpDirNative + "/" + lStamp + ".txt";

        // 1. The report file: what happened and the raw stack. open, write and
        //    backtrace_symbols_fd do not allocate.
        void*     lFrames[MAX_STACK_FRAMES];
        const int lFrameCount = backtrace(lFrames, MAX_STACK_FRAMES);

        const int  lFd       = open(lReportPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        const bool lbWritten = lFd >= 0;
        if (lbWritten)
        {
            WriteText(lFd, "Opaax crash report\n");
            WriteText(lFd, lSignal != 0 ? DescribeSignal(lSignal) : "report requested (no crash)");
            WriteText(lFd, "\n\nStack:\n");
            backtrace_symbols_fd(lFrames, lFrameCount, lFd);
            close(lFd);
        }

        // 2. The stack, in the log.
        if (lSignal != 0)
        {
            OPAAX_LOG(LogCrashHandler, Critical, "CRASH — {} (signal {})", DescribeSignal(lSignal), lSignal);
        }
        else
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Report requested — the calling thread's stack");
        }

        if (char** lSymbols = backtrace_symbols(lFrames, lFrameCount))
        {
            for (int i = 0; i < lFrameCount; ++i)
            {
                OPAAX_LOG(LogCrashHandler, Critical, "  #{:<2} {}", i, lSymbols[i]);
            }
            std::free(lSymbols);
        }

        const OpaaxString lReportUtf8 = lbWritten ? Utf8::FromFsPath(std::filesystem::path(lReportPath))
                                                  : OpaaxString();
        if (lbWritten)
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Report written: {}", lReportUtf8);
        }
        else
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Report could NOT be written to {}", m_Settings.DumpDir);
        }

        // 3. The log next to the report (the next launch overwrites the original).
        Logger::Get().Flush();
        if (!m_LogFileNative.empty())
        {
            std::error_code lError;
            std::filesystem::copy_file(std::filesystem::path(m_LogFileNative),
                                       std::filesystem::path(m_DumpDirNative + "/" + lStamp + ".log"),
                                       std::filesystem::copy_options::overwrite_existing, lError);
        }

        return lReportUtf8;
    }

    long CrashHandler::HandleCrash(void* InSignal)
    {
        // A crash inside the report (or a second crashing thread) must not recurse.
        if (m_bHandling.test_and_set())
        {
            return 0;
        }

        const OpaaxString lReport = WriteReport(InSignal);

        // No native dialog here: the report path goes to stderr.
        if (m_Settings.bShowDialog)
        {
            std::fprintf(stderr, "Opaax crashed. Report: %s\n",
                         lReport.IsEmpty() ? "(none written, see the log)" : lReport.CStr());
        }

        return 0;
    }
}

#endif // OPAAX_PLATFORM_POSIX
