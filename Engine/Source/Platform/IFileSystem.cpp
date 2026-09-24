#include "IFileSystem.h"

#include "Core/Log/Logger.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullFileSystem — every primitive fails, nothing reaches a disk. Silent by design: "there is
        // no filesystem" is the answer, not an error, and callers already branch on false.
        // =====================================================================
        class NullFileSystem final : public IFileSystem
        {
        public:
            bool CreateDirectories(const OpaaxString&) const override { return false; }
            bool IsPathExist(const OpaaxString&)       const override { return false; }

            bool ListDirectory(const OpaaxString&, TDynArray<Entry>&) const override { return false; }
        };
    }

    const IFileSystem& IFileSystem::Null()
    {
        static NullFileSystem s_Null;
        return s_Null;
    }

    OpaaxString IFileSystem::GetPathIfNCreate(const OpaaxString& InPath) const
    {
        if (InPath.IsEmpty())
        {
            return {};
        }

        if (CreateDirectories(InPath))
        {
            return InPath;
        }

        // Reachable before Logger::Init (Bootstrap creates directories): the line is held and replayed.
        OPAAX_APP_LOG(Error, "IFileSystem cannot create: {}", InPath.CStr());

        return {};
    }
}
