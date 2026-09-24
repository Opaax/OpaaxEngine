#include "IFileSystem.h"

#include <iostream>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"

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

        // Reachable before the logger is provided (Bootstrap creates directories), hence the fallback.
        ILogger& Logger = OpaaxApplication::GetAppService<ILogger>();

        if (!Logger.IsNull())
        {
            OPAAX_APP_LOG(Error,"IFileSystem cannot create: {}", InPath.CStr());
        }
        else
        {
            std::cout << "IFileSystem cannot create: "<< InPath.CStr() << std::endl;
        }

        return {};
    }
}
