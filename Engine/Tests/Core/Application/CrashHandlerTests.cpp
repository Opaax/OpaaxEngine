// Suite: CrashHandler — the report path on an OWN instance (SG4). Nothing is installed: the hooks
// are process-wide and a test runner must keep the OS default. A real crash is proven by the
// dev-only `--crash-test` smoke run instead.
#include <doctest.h>

#include "Platform/CrashHandler.h"
#include "Core/String/OpaaxUtf8.h"   // I7 — never an fs::path from CStr()

#ifdef OPAAX_PLATFORM_WINDOWS

#include <filesystem>
#include <fstream>
#include <string>

using namespace Opaax;

TEST_CASE("CrashHandler: WriteReport writes a minidump and a copy of the log, without installing")
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

        const OpaaxString lDump = lHandler.WriteReport(nullptr);

        CHECK_FALSE(lHandler.IsInstalled());
        REQUIRE_FALSE(lDump.IsEmpty());

        const fs::path lDumpPath = Utf8::ToFsPath(lDump);
        CHECK(fs::exists(lDumpPath));
        CHECK(fs::file_size(lDumpPath) > 0);

        fs::path lLogCopy = lDumpPath;
        lLogCopy.replace_extension(".log");
        REQUIRE(fs::exists(lLogCopy));

        std::ifstream lCopy(lLogCopy);
        const std::string lText((std::istreambuf_iterator<char>(lCopy)), std::istreambuf_iterator<char>());
        CHECK(lText == "the session so far");
    }

    fs::remove_all(lRoot, lError);
}

#endif
