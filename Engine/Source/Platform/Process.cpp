#include "Platform/Process.h"

#include <cstdlib>

#ifdef OPAAX_PLATFORM_POSIX
#include <sys/wait.h>
#else
#include "Core/String/OpaaxUtf8.h"
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
}
