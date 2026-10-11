#include "Platform/IFileSystem.h"

#include "Core/Log/Logger.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullFileSystem — every call fails, no disk access. No log: callers handle false.
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

        // May run before Logger::Init: the line is held and replayed.
        OPAAX_APP_LOG(Error, "IFileSystem cannot create: {}", InPath.CStr());

        return {};
    }
}
