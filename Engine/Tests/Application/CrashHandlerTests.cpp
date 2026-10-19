// Suite: CrashHandler — the report on a local instance. Nothing is installed (the hooks are
// process-wide). A real crash is tested with the dev-only --crash-test run.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"
#include "Platform/CrashHandler.h"

using namespace Opaax;

TEST_CASE("CrashHandler: WriteReport writes a report and a copy of the log, without installing")
{
    const TestTempDir lRoot("CrashHandler");

    const OpaaxString lLog = lRoot.Sub("live.log");
    FileIO::WriteAllText(lLog, "the session so far");

    {
        CrashHandler lHandler;
        lHandler.Configure({ lRoot.Sub("Crashes"), lLog, false });

        const OpaaxString lReport = lHandler.WriteReport(nullptr);

        CHECK_FALSE(lHandler.IsInstalled());
        REQUIRE_FALSE(lReport.IsEmpty());

        // A minidump on Windows, a text stack trace elsewhere.
        TDynArray<Uint8> lReportBytes;
        CHECK(FileIO::ReadAllBytes(lReport, lReportBytes));
        CHECK_FALSE(lReportBytes.empty());
#ifdef OPAAX_PLATFORM_WINDOWS
        CHECK(PathString::Extension(lReport) == ".dmp");
#else
        CHECK(PathString::Extension(lReport) == ".txt");
#endif

        const OpaaxString lLogCopy = PathString::ReplaceExtension(lReport, ".log");
        REQUIRE(FileSystem::IsFile(lLogCopy));
        CHECK(FileIO::ReadAllText(lLogCopy) == "the session so far");
    }
}
