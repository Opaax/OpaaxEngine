#include "Platform/Process.h"

#include <cstdlib>

#include "Core/IO/FileSystem.h"

#ifdef OPAAX_PLATFORM_POSIX
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#else
#include "Core/String/OpaaxUtf8.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Opaax::Process
{
    void Abort()
    {
        std::abort();
    }

    Int32 RunCommandLine(const OpaaxString& InCommandLine)
    {
#ifdef OPAAX_PLATFORM_POSIX
        const int lStatus = std::system(InCommandLine.CStr());

        // A wait status there: the exit code when the shell exited, -1 when a signal ended it.
        return (lStatus != -1 && WIFEXITED(lStatus)) ? static_cast<Int32>(WEXITSTATUS(lStatus)) : -1;
#else
        // The wide call: the narrow one reads its text in the ANSI code page, not UTF-8.
        return static_cast<Int32>(_wsystem(Utf8::ToWide(InCommandLine).c_str()));
#endif
    }

    bool OpenFolder(const OpaaxString& InFolder)
    {
        if (!FileSystem::IsDirectory(InFolder))
        {
            return false;
        }

#ifdef OPAAX_PLATFORM_POSIX
#if defined(OPAAX_PLATFORM_MACOS)
        const char* const lTool = "open";
#else
        const char* const lTool = "xdg-open";
#endif
        // A shell starts the tool in the background and ends at once, so the caller never waits for a
        // file manager; the folder goes as an argument, never through the shell's quoting.
        const char* const lArgs[] = { "sh", "-c", "\"$0\" \"$1\" >/dev/null 2>&1 &", lTool, InFolder.CStr(), nullptr };

        pid_t lShell = 0;
        if (posix_spawn(&lShell, "/bin/sh", nullptr, nullptr, const_cast<char* const*>(lArgs), environ) != 0)
        {
            return false;
        }

        int lStatus = 0;
        return waitpid(lShell, &lStatus, 0) == lShell && WIFEXITED(lStatus) && WEXITSTATUS(lStatus) == 0;
#else
        // Backslashes: a path given to the shell is read as Explorer reads it.
        std::wstring lWide = Utf8::ToWide(InFolder);
        for (wchar_t& lChar : lWide)
        {
            if (lChar == L'/') { lChar = L'\\'; }
        }

        const HINSTANCE lResult = ShellExecuteW(nullptr, L"open", lWide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

        // A value above 32 is success (the API's old convention).
        return reinterpret_cast<INT_PTR>(lResult) > 32;
#endif
    }

    TOptional<OpaaxString> GetEnvironment(const OpaaxString& InName)
    {
        if (InName.IsEmpty())
        {
            return NullOpt;
        }

#ifdef OPAAX_PLATFORM_POSIX
        const char* const lValue = std::getenv(InName.CStr());
        if (lValue == nullptr)
        {
            return NullOpt;
        }
        return OpaaxString(lValue);
#else
        // The process's own block, which SetEnvironment changes along with the C runtime's copy.
        const std::wstring lName = Utf8::ToWide(InName);
        const DWORD lSize = GetEnvironmentVariableW(lName.c_str(), nullptr, 0);
        if (lSize == 0)
        {
            return NullOpt;
        }

        std::wstring lValue(lSize, L'\0');
        const DWORD lLength = GetEnvironmentVariableW(lName.c_str(), lValue.data(), lSize);
        lValue.resize(lLength);
        return Utf8::FromWide(lValue);
#endif
    }

    bool SetEnvironment(const OpaaxString& InName, const OpaaxString& InValue)
    {
        if (InName.IsEmpty() || InName.Find('=') >= 0)
        {
            return false;
        }

#ifdef OPAAX_PLATFORM_POSIX
        return InValue.IsEmpty() ? unsetenv(InName.CStr()) == 0 : setenv(InName.CStr(), InValue.CStr(), 1) == 0;
#else
        // The C runtime's call: it changes its own copy (getenv) and the process's block (what a DLL
        // reading GetEnvironmentVariable sees) together. An empty value removes the variable.
        return _wputenv_s(Utf8::ToWide(InName).c_str(), Utf8::ToWide(InValue).c_str()) == 0;
#endif
    }
}
