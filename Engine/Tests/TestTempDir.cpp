#include "TestTempDir.h"

#include "Core/IO/FileSystem.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/Time/Clock.h"

namespace Opaax
{
    namespace
    {
        /** Two directories made in the same clock tick still differ. */
        Uint32 NextId()
        {
            static TAtomic<Uint32> s_Next{ 0 };
            return s_Next.fetch_add(1);
        }
    }

    TestTempDir::TestTempDir(const char* InTag)
        : m_Path(PathString::Join(FileSystem::TempDirectory(),
                                  OpaaxString::Format("Opaax_{}_{}_{}", InTag, Clock::NowNanoseconds(), NextId())))
    {
        FileSystem::RemoveAll(m_Path);
        FileSystem::CreateDirectories(m_Path);
    }

    TestTempDir::~TestTempDir()
    {
        FileSystem::RemoveAll(m_Path);
    }

    OpaaxString TestTempDir::Sub(const OpaaxStringView InRelative) const
    {
        return PathString::Join(m_Path, InRelative);
    }
}
