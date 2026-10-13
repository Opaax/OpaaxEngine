#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // CommandLine — the program's arguments, in UTF-8 on every platform. On Windows they are read
    //   from the wide command line: main's argv is in the system's code page, which cannot hold
    //   every character (a folder named in Japanese on a Western Windows arrives as "??").
    // =============================================================================
    class CommandLine
    {
    public:
        CommandLine() = default;

        /** InArguments as given, the program first: { "Game.exe", "--capture", "shot.png" }. */
        explicit CommandLine(TDynArray<OpaaxString> InArguments);

        /** The running program's arguments. InArgc and InArgv are main's; Windows reads its wide command line instead. */
        static CommandLine FromMain(int InArgc, char** InArgv);

#ifdef OPAAX_PLATFORM_WINDOWS
        /** A Windows command line split the way the C runtime splits it, each argument in UTF-8. */
        static CommandLine FromWide(const wchar_t* InCommandLine);
#endif

        /** Whether InFlag ("--crash-test") is one of the arguments after the program. */
        bool HasFlag(OpaaxStringView InFlag) const;

        /** The argument after InFlag ("--capture shot.png": "shot.png"), or null when InFlag is absent or last. */
        const OpaaxString* FindValue(OpaaxStringView InFlag) const;

        /** FindValue, or empty. */
        OpaaxString GetValue(OpaaxStringView InFlag) const;

        /** Every argument, the program first. */
        const TDynArray<OpaaxString>& GetArguments() const noexcept { return m_Arguments; }

    private:
        TDynArray<OpaaxString> m_Arguments;
    };
}
