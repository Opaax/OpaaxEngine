#include "Platform/CrashHandler.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxUtf8.h"   // I7 — the one UTF-8 <-> UTF-16 idiom

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <DbgHelp.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(CrashHandler);

    namespace
    {
        // Raised by the non-SEH hooks so every failure reaches the filter with a real context.
        constexpr DWORD EXCEPTION_OPAAX_TERMINATE     = 0xE0A70001;
        constexpr DWORD EXCEPTION_OPAAX_ABORT         = 0xE0A70002;
        constexpr DWORD EXCEPTION_OPAAX_PURE_CALL     = 0xE0A70003;
        constexpr DWORD EXCEPTION_OPAAX_INVALID_PARAM = 0xE0A70004;

        // Headroom for the handler itself when the crash IS a stack overflow.
        constexpr ULONG STACK_GUARANTEE_BYTES = 64 * 1024;
        constexpr int   MAX_STACK_FRAMES      = 64;

        const char* DescribeCode(const DWORD InCode)
        {
            switch (InCode)
            {
            case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
            case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
            case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
            case EXCEPTION_BREAKPOINT:            return "breakpoint (an assert with no debugger)";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
            case EXCEPTION_IN_PAGE_ERROR:         return "in-page error";
            case EXCEPTION_OPAAX_TERMINATE:       return "std::terminate (uncaught C++ exception?)";
            case EXCEPTION_OPAAX_ABORT:           return "abort()";
            case EXCEPTION_OPAAX_PURE_CALL:       return "pure virtual call";
            case EXCEPTION_OPAAX_INVALID_PARAM:   return "invalid parameter to a CRT function";
            default:                              return "unknown exception";
            }
        }

        LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* InPointers)
        {
            return CrashHandler::Get().HandleCrash(InPointers);
        }

        [[noreturn]] void RaiseAsCrash(const DWORD InCode)
        {
            RaiseException(InCode, EXCEPTION_NONCONTINUABLE, 0, nullptr);
            std::_Exit(3);
        }

        void OnTerminate()                { RaiseAsCrash(EXCEPTION_OPAAX_TERMINATE); }
        void OnAbortSignal(int)           { RaiseAsCrash(EXCEPTION_OPAAX_ABORT); }
        void OnPureCall()                 { RaiseAsCrash(EXCEPTION_OPAAX_PURE_CALL); }
        void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
        {
            RaiseAsCrash(EXCEPTION_OPAAX_INVALID_PARAM);
        }

        /** Opaax_yyyymmdd_hhmmss — no heap, the crash may have corrupted it. */
        void FormatStamp(wchar_t (&OutStamp)[32])
        {
            SYSTEMTIME lNow;
            GetLocalTime(&lNow);
            swprintf_s(OutStamp, L"Opaax_%04u%02u%02u_%02u%02u%02u", lNow.wYear, lNow.wMonth, lNow.wDay,
                       lNow.wHour, lNow.wMinute, lNow.wSecond);
        }

        void LogStack(const CONTEXT& InContext)
        {
            const HANDLE lProcess = GetCurrentProcess();
            const HANDLE lThread  = GetCurrentThread();

            CONTEXT      lContext = InContext;   // StackWalk64 walks it in place
            STACKFRAME64 lFrame{};
            lFrame.AddrPC.Offset    = lContext.Rip;
            lFrame.AddrPC.Mode      = AddrModeFlat;
            lFrame.AddrFrame.Offset = lContext.Rbp;
            lFrame.AddrFrame.Mode   = AddrModeFlat;
            lFrame.AddrStack.Offset = lContext.Rsp;
            lFrame.AddrStack.Mode   = AddrModeFlat;

            alignas(SYMBOL_INFO) char lSymbolStorage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
            SYMBOL_INFO* lSymbol  = reinterpret_cast<SYMBOL_INFO*>(lSymbolStorage);

            for (int i = 0; i < MAX_STACK_FRAMES; ++i)
            {
                if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, lProcess, lThread, &lFrame, &lContext, nullptr,
                                 SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
                    || lFrame.AddrPC.Offset == 0)
                {
                    break;
                }

                const DWORD64 lAddress = lFrame.AddrPC.Offset;

                char lModule[MAX_PATH] = "?";
                if (const DWORD64 lBase = SymGetModuleBase64(lProcess, lAddress))
                {
                    char lModulePath[MAX_PATH];
                    if (GetModuleFileNameA(reinterpret_cast<HMODULE>(lBase), lModulePath, MAX_PATH) > 0)
                    {
                        const char* lSlash = strrchr(lModulePath, '\\');
                        strcpy_s(lModule, lSlash ? lSlash + 1 : lModulePath);
                    }
                }

                ZeroMemory(lSymbolStorage, sizeof(lSymbolStorage));
                lSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                lSymbol->MaxNameLen   = MAX_SYM_NAME;

                DWORD64 lSymbolOffset = 0;
                const bool lbHasSymbol = SymFromAddr(lProcess, lAddress, &lSymbolOffset, lSymbol) != FALSE;

                IMAGEHLP_LINE64 lLine{};
                lLine.SizeOfStruct = sizeof(lLine);
                DWORD lLineOffset  = 0;
                const bool lbHasLine = SymGetLineFromAddr64(lProcess, lAddress, &lLineOffset, &lLine) != FALSE;

                if (lbHasLine)
                {
                    OPAAX_LOG(LogCrashHandler, Critical, "  #{:<2} {}!{} ({}:{})", i, lModule,
                              lbHasSymbol ? lSymbol->Name : "?", lLine.FileName, lLine.LineNumber);
                }
                else
                {
                    OPAAX_LOG(LogCrashHandler, Critical, "  #{:<2} {}!{} +0x{:X}", i, lModule,
                              lbHasSymbol ? lSymbol->Name : "?", lSymbolOffset);
                }
            }
        }
    }

    CrashHandler& CrashHandler::Get()
    {
        // Leaked on purpose (SG5): a crash during static destruction must still find it.
        static CrashHandler* s_Instance = new CrashHandler();
        return *s_Instance;
    }

    CrashHandler::CrashHandler() = default;

    CrashHandler::~CrashHandler()
    {
        // Only a test's own instance gets here; Get()'s is leaked.
        Uninstall();

        if (m_bSymbolsReady)
        {
            SymCleanup(GetCurrentProcess());
        }
    }

    void CrashHandler::TriggerTestCrash()
    {
        OPAAX_LOG(LogCrashHandler, Warn, "--crash-test: writing through a null pointer on purpose");

        volatile int* lNull = nullptr;
        *lNull = 0x0DEAD;
    }

    void CrashHandler::Configure(const CrashHandlerSettings& InSettings)
    {
        m_Settings    = InSettings;
        m_DumpDirWide = Utf8::ToWide(InSettings.DumpDir);
        m_LogFileWide = Utf8::ToWide(InSettings.LogFile);

        std::error_code lError;
        std::filesystem::create_directories(std::filesystem::path(m_DumpDirWide), lError);

        if (!m_bSymbolsReady)
        {
            // At configure time, never at crash time: loading symbols allocates and reads disk.
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
            m_bSymbolsReady = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
        }
    }

    void CrashHandler::Install(const CrashHandlerSettings& InSettings)
    {
        Configure(InSettings);

        if (m_bInstalled) { return; }

        ULONG lGuarantee = STACK_GUARANTEE_BYTES;
        SetThreadStackGuarantee(&lGuarantee);

        m_PreviousFilter = reinterpret_cast<void*>(SetUnhandledExceptionFilter(&OnUnhandledException));
        std::set_terminate(&OnTerminate);
        std::signal(SIGABRT, &OnAbortSignal);
        _set_purecall_handler(&OnPureCall);
        _set_invalid_parameter_handler(&OnInvalidParameter);

        m_bInstalled = true;

        OPAAX_LOG(LogCrashHandler, Info, "Crash handler installed — dumps to {} (symbols {})",
                  m_Settings.DumpDir, m_bSymbolsReady ? "ready" : "UNAVAILABLE");
    }

    void CrashHandler::Uninstall()
    {
        if (!m_bInstalled) { return; }

        SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(m_PreviousFilter));
        std::set_terminate(nullptr);
        std::signal(SIGABRT, SIG_DFL);
        _set_purecall_handler(nullptr);
        _set_invalid_parameter_handler(nullptr);

        if (m_bSymbolsReady)
        {
            SymCleanup(GetCurrentProcess());
            m_bSymbolsReady = false;
        }

        m_bInstalled = false;
    }

    OpaaxString CrashHandler::WriteReport(void* InExceptionPointers)
    {
        EXCEPTION_POINTERS* lPointers = static_cast<EXCEPTION_POINTERS*>(InExceptionPointers);

        wchar_t lStamp[32];
        FormatStamp(lStamp);

        const std::wstring lDumpPath = m_DumpDirWide + L"/" + lStamp + L".dmp";

        // 1. The dump — the one artefact that survives a corrupted heap.
        bool lbDumped = false;
        const HANDLE lFile = CreateFileW(lDumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION lInfo{};
            lInfo.ThreadId          = GetCurrentThreadId();
            lInfo.ExceptionPointers = lPointers;
            lInfo.ClientPointers    = FALSE;

            const MINIDUMP_TYPE lType = static_cast<MINIDUMP_TYPE>(
                MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithDataSegs);

            lbDumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), lFile, lType,
                                         lPointers ? &lInfo : nullptr, nullptr, nullptr) != FALSE;
            CloseHandle(lFile);
        }

        // 2. What happened and where, in the log.
        CONTEXT lContext{};
        if (lPointers != nullptr)
        {
            const EXCEPTION_RECORD& lRecord = *lPointers->ExceptionRecord;
            OPAAX_LOG(LogCrashHandler, Critical, "CRASH — {} (0x{:08X}) at 0x{:X}", DescribeCode(lRecord.ExceptionCode),
                      lRecord.ExceptionCode, reinterpret_cast<uintptr_t>(lRecord.ExceptionAddress));

            if (lRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && lRecord.NumberParameters >= 2)
            {
                OPAAX_LOG(LogCrashHandler, Critical, "  {} address 0x{:X}",
                          lRecord.ExceptionInformation[0] == 1 ? "writing" : "reading", lRecord.ExceptionInformation[1]);
            }

            lContext = *lPointers->ContextRecord;
        }
        else
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Report requested — the calling thread's stack");
            RtlCaptureContext(&lContext);
        }

        if (m_bSymbolsReady)
        {
            LogStack(lContext);
        }

        const OpaaxString lDumpPathUtf8 = lbDumped ? Utf8::FromWide(lDumpPath) : OpaaxString();
        if (lbDumped)
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Dump written: {}", lDumpPathUtf8);
        }
        else
        {
            OPAAX_LOG(LogCrashHandler, Critical, "Dump could NOT be written to {}", m_Settings.DumpDir);
        }

        // 3. The log beside the dump: the next launch truncates the original.
        Logger::Get().Flush();
        if (!m_LogFileWide.empty())
        {
            const std::wstring lLogCopy = m_DumpDirWide + L"/" + lStamp + L".log";
            CopyFileW(m_LogFileWide.c_str(), lLogCopy.c_str(), FALSE);
        }

        return lDumpPathUtf8;
    }

    long CrashHandler::HandleCrash(void* InExceptionPointers)
    {
        // A crash inside the report (or a second thread crashing) must not recurse.
        if (m_bHandling.test_and_set())
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }

        const OpaaxString lDumpPath = WriteReport(InExceptionPointers);

        if (m_Settings.bShowDialog)
        {
            const std::wstring lText = lDumpPath.IsEmpty()
                ? std::wstring(L"Opaax crashed.\n\nNo crash dump could be written. The log holds the call stack.")
                : L"Opaax crashed.\n\nA crash dump and a copy of the log were written to:\n" + Utf8::ToWide(lDumpPath);

            MessageBoxW(nullptr, lText.c_str(), L"Opaax crashed", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
