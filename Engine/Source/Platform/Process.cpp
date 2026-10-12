#include "Platform/Process.h"

#include <cstdlib>

#ifdef OPAAX_PLATFORM_POSIX
#include <sys/wait.h>
#endif

namespace Opaax::Process
{
    void Abort()
    {
        std::abort();
    }

    Int32 RunCommandLine(const OpaaxString& InCommandLine)
    {
        const int lStatus = std::system(InCommandLine.CStr());

#ifdef OPAAX_PLATFORM_POSIX
        // A wait status there: the exit code when the shell exited, -1 when a signal ended it.
        return (lStatus != -1 && WIFEXITED(lStatus)) ? static_cast<Int32>(WEXITSTATUS(lStatus)) : -1;
#else
        return static_cast<Int32>(lStatus);
#endif
    }
}
