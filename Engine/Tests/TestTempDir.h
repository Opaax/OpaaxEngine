// TestTempDir.h — a directory of its own under the system's temp folder for one test case: created
//   empty, removed when the case ends (a REQUIRE that throws included).
#pragma once

#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Time/Clock.h"

namespace Opaax
{
    class TestTempDir
    {
    public:
        /** InTag names the directory, for whoever looks at the temp folder while a test runs. */
        explicit TestTempDir(const char* InTag)
            : m_Path(PathString::Join(FileSystem::TempDirectory(),
                                      OpaaxString::Format("Opaax_{}_{}", InTag, Clock::NowNanoseconds())))
        {
            FileSystem::RemoveAll(m_Path);
            FileSystem::CreateDirectories(m_Path);
        }

        ~TestTempDir() { FileSystem::RemoveAll(m_Path); }

        TestTempDir(const TestTempDir&)            = delete;
        TestTempDir& operator=(const TestTempDir&) = delete;

        /** The directory, with forward slashes. */
        const OpaaxString& Path() const noexcept { return m_Path; }

        /** InRelative under the directory: Sub("Maps/A.opaaxmap"). */
        OpaaxString Sub(const OpaaxStringView InRelative) const { return PathString::Join(m_Path, InRelative); }

    private:
        OpaaxString m_Path;
    };
}
