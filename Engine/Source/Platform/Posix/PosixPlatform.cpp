#include "Platform/Posix/PosixPlatform.h"

#ifdef OPAAX_PLATFORM_POSIX

#include <chrono>
#include <climits>
#include <cstdlib>
#include <string>
#include <thread>

#include <unistd.h>

#if defined(OPAAX_PLATFORM_MACOS)
#include <mach-o/dyld.h>
#endif

namespace Opaax
{
    Uint32 PosixPlatform::GetLogicalCoreCount() const
    {
        const unsigned int lCount = Thread::hardware_concurrency();
        return lCount == 0u ? 1u : static_cast<Uint32>(lCount);
    }

    double PosixPlatform::GetTimeSeconds() const
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    OpaaxString PosixPlatform::GetExecutablePath() const
    {
#if defined(OPAAX_PLATFORM_LINUX)
        // The link target of /proc/self/exe; grow the buffer until it fits.
        std::string lPath(PATH_MAX, '\0');
        for (;;)
        {
            const ssize_t lLen = readlink("/proc/self/exe", lPath.data(), lPath.size());
            if (lLen < 0)                                    { return OpaaxString(); }
            if (static_cast<size_t>(lLen) < lPath.size())    { lPath.resize(static_cast<size_t>(lLen)); break; }
            lPath.resize(lPath.size() * 2);
        }
        return OpaaxString(lPath.c_str());
#elif defined(OPAAX_PLATFORM_MACOS)
        uint32_t lSize = 0;
        _NSGetExecutablePath(nullptr, &lSize);

        std::string lRaw(lSize, '\0');
        if (_NSGetExecutablePath(lRaw.data(), &lSize) != 0)
        {
            return OpaaxString();
        }

        // Resolve symlinks and "..": the raw path is whatever the process was launched with.
        char lResolved[PATH_MAX];
        if (realpath(lRaw.c_str(), lResolved) == nullptr)
        {
            return OpaaxString(lRaw.c_str());
        }
        return OpaaxString(lResolved);
#endif
    }

    OpaaxString PosixPlatform::GetPlatformName() const
    {
#if defined(OPAAX_PLATFORM_LINUX)
        return OpaaxString("Linux");
#else
        return OpaaxString("macOS");
#endif
    }
}

#endif // OPAAX_PLATFORM_POSIX
