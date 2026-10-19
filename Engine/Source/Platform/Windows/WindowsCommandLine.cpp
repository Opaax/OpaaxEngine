#include "Platform/CommandLine.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Core/String/OpaaxUtf8.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>

namespace Opaax
{
    CommandLine CommandLine::FromMain(const int InArgc, char** InArgv)
    {
        CommandLine lCommandLine = FromWide(GetCommandLineW());
        if (!lCommandLine.GetArguments().empty())
        {
            return lCommandLine;
        }

        // Only if Windows could not split its own command line: main's, as it came.
        TDynArray<OpaaxString> lArguments;
        for (int lIndex = 0; lIndex < InArgc; ++lIndex)
        {
            lArguments.push_back(OpaaxString(InArgv[lIndex]));
        }
        return CommandLine(Move(lArguments));
    }

    CommandLine CommandLine::FromWide(const wchar_t* InCommandLine)
    {
        int     lCount = 0;
        LPWSTR* lWide  = CommandLineToArgvW(InCommandLine, &lCount);
        if (lWide == nullptr)
        {
            return CommandLine();
        }

        TDynArray<OpaaxString> lArguments;
        lArguments.reserve(static_cast<Uint64>(lCount));
        for (int lIndex = 0; lIndex < lCount; ++lIndex)
        {
            lArguments.push_back(Utf8::FromWide(lWide[lIndex]));
        }
        LocalFree(lWide);

        return CommandLine(Move(lArguments));
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
