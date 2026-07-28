#pragma once

#include "Application/Services/Platforms/IFileSystem.h"
#include "Core/EngineAPI.h"

#ifdef OPAAX_PLATFORM_WINDOWS

namespace Opaax
{
    // =============================================================================
    // WindowsFileSystem — IFileSystem for Windows. Held BY VALUE by WindowsPlatform.
    //
    //   Its entire reason to exist is the ENCODING BOUNDARY. The engine's paths are UTF-8
    //   (WindowsPlatform::GetExecutablePath converts through CP_UTF8), while Windows is UTF-16 —
    //   and every narrow shortcut in between silently decodes as the ANSI code page instead. So each
    //   entry point converts UTF-8 -> UTF-16 on the way in and UTF-16 -> UTF-8 on the way out, and the
    //   filesystem work in between runs on wide paths only.
    //
    //   The work itself is std::filesystem, deliberately: on a wide path it issues the same syscalls a
    //   hand-rolled FindFirstFileW loop would, and re-implementing recursive create + mid-walk error
    //   recovery by hand would buy nothing but a second chance to get them wrong.
    // =============================================================================
    class OPAAX_API WindowsFileSystem final : public IFileSystem
    {
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IFileSystem interface
    public:
        bool CreateDirectories(const OpaaxString& InPath)   const override;
        bool IsPathExist(const OpaaxString& InPath)         const override;
        bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const override;
        //~End IFileSystem interface
    };
}

#endif // OPAAX_PLATFORM_WINDOWS
