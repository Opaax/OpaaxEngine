// TestTempDir.h — a directory of its own under the system's temp folder for one test case: created
//   empty, removed when the case ends (a REQUIRE that throws included).
#pragma once

#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class TestTempDir
    {
    public:
        /** InTag names the directory, for whoever looks at the temp folder while a test runs. */
        explicit TestTempDir(const char* InTag);
        ~TestTempDir();

        TestTempDir(const TestTempDir&)            = delete;
        TestTempDir& operator=(const TestTempDir&) = delete;

        /** The directory, with forward slashes. */
        const OpaaxString& Path() const noexcept { return m_Path; }

        /** InRelative under the directory: Sub("Maps/A.opaaxmap"). */
        OpaaxString Sub(OpaaxStringView InRelative) const;

    private:
        OpaaxString m_Path;
    };
}
