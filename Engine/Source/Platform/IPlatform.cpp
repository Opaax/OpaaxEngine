#include "IPlatform.h"

#include "Core/String/OpaaxPathString.h"
#include "Platform/IFileSystem.h"

namespace Opaax
{
    namespace
    {
        class NullPlatform final : public IPlatform
        {
        public:
            bool                IsNull()                const noexcept override { return true; }
            Uint32              GetLogicalCoreCount()   const override { return 1; }
            double              GetTimeSeconds()        const override { return 0.0; }
            OpaaxString         GetExecutablePath()     const override { return OpaaxString("Null Exec Path"); }
            OpaaxString         GetPlatformName()       const override { return OpaaxString("Null Platform"); }
            // A null platform does not touch the disk.
            const IFileSystem&  GetFileSystem()         const override { return IFileSystem::Null(); }
            OpaaxString         GetUserDataDirectory()  const override { return OpaaxString(); }
        };
    }

    OpaaxString ResolvePosixUserDataDirectory(const OpaaxString& InXdgDataHome, const OpaaxString& InHome, bool bInMacOS)
    {
        if (bInMacOS)
        {
            return InHome.IsEmpty() ? OpaaxString() : PathString::Join(InHome, "Library/Application Support");
        }

        // The XDG spec ignores a relative value. A POSIX test, not PathString::IsAbsolute: the rule is
        // the same when the tests run on Windows.
        if (InXdgDataHome.StartsWith("/"))
        {
            return InXdgDataHome;
        }

        return InHome.IsEmpty() ? OpaaxString() : PathString::Join(InHome, ".local/share");
    }

    ServiceTypeID IPlatform::StaticTypeID() noexcept { 
		static const int s_Tag = 0; return reinterpret_cast<ServiceTypeID>(&s_Tag); 
	}

    IPlatform& IPlatform::Null() 
    { 
		static NullPlatform s_Null; 
        return s_Null; 
	}   // defined here: one instance
}
