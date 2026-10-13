#include "Platform/CommandLine.h"

#ifdef OPAAX_PLATFORM_POSIX

namespace Opaax
{
    CommandLine CommandLine::FromMain(const int InArgc, char** InArgv)
    {
        // Linux and macOS hand main the bytes as given: UTF-8 on any current system.
        TDynArray<OpaaxString> lArguments;
        lArguments.reserve(static_cast<Uint64>(InArgc > 0 ? InArgc : 0));
        for (int lIndex = 0; lIndex < InArgc; ++lIndex)
        {
            lArguments.push_back(OpaaxString(InArgv[lIndex]));
        }
        return CommandLine(Move(lArguments));
    }
}

#endif // OPAAX_PLATFORM_POSIX
