// Suite: CrashHandler — the report on a local instance. Nothing is installed (the hooks are
// process-wide). A real crash is tested with the dev-only --crash-test run.
#include <doctest.h>

#include "Platform/CrashHandler.h"
#include "Core/String/OpaaxUtf8.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace Opaax;

TEST_CASE("CrashHandler: WriteReport writes a report and a copy of the log, without installing")
{
    namespace fs = std::filesystem;

    const fs::path lRoot = fs::temp_directory_path() / "OpaaxCrashHandlerTests";
    std::error_code lError;
    fs::remove_all(lRoot, lError);
    fs::create_directories(lRoot, lError);

    const fs::path lLog = lRoot / "live.log";
    {
        std::ofstream lFile(lLog);
        lFile << "the session so far";
    }

    {
        CrashHandler lHandler;
        lHandler.Configure({ Utf8::FromFsPath(lRoot / "Crashes"), Utf8::FromFsPath(lLog), false });

        const OpaaxString lReport = lHandler.WriteReport(nullptr);

        CHECK_FALSE(lHandler.IsInstalled());
        REQUIRE_FALSE(lReport.IsEmpty());

        // A minidump on Windows, a text stack trace elsewhere.
        const fs::path lReportPath = Utf8::ToFsPath(lReport);
        CHECK(fs::exists(lReportPath));
        CHECK(fs::file_size(lReportPath) > 0);
#ifdef OPAAX_PLATFORM_WINDOWS
        CHECK(lReportPath.extension() == ".dmp");
#else
        CHECK(lReportPath.extension() == ".txt");
#endif

        fs::path lLogCopy = lReportPath;
        lLogCopy.replace_extension(".log");
        REQUIRE(fs::exists(lLogCopy));

        std::ifstream lCopy(lLogCopy);
        const std::string lText((std::istreambuf_iterator<char>(lCopy)), std::istreambuf_iterator<char>());
        CHECK(lText == "the session so far");
    }

    fs::remove_all(lRoot, lError);
}
