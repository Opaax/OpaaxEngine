// Suite: CommandLine — the program's arguments in UTF-8: flags and the value after a flag, and on
// Windows the wide command line, split the way the C runtime splits it.
#include <doctest.h>

#include "Platform/CommandLine.h"

using namespace Opaax;

TEST_CASE("CommandLine: flags and the values after them; the program itself is never a flag")
{
    const CommandLine lLine({ "--crash-test", "--capture", "shot.png", "--exec" });

    CHECK_FALSE(lLine.HasFlag("--crash-test"));   // the first argument is the program
    CHECK(lLine.HasFlag("--capture"));
    CHECK_FALSE(lLine.HasFlag("--cap"));          // whole arguments only

    REQUIRE(lLine.FindValue("--capture") != nullptr);
    CHECK(*lLine.FindValue("--capture") == "shot.png");
    CHECK(lLine.GetValue("--capture") == "shot.png");

    // A flag that ends the line has no value; an absent one neither.
    CHECK(lLine.HasFlag("--exec"));
    CHECK(lLine.FindValue("--exec") == nullptr);
    CHECK(lLine.FindValue("--project") == nullptr);
    CHECK(lLine.GetValue("--project").IsEmpty());

    const CommandLine lEmpty;
    CHECK(lEmpty.GetArguments().empty());
    CHECK_FALSE(lEmpty.HasFlag("--capture"));
    CHECK(lEmpty.FindValue("--capture") == nullptr);
}

#ifdef OPAAX_PLATFORM_POSIX
TEST_CASE("CommandLine: on Linux and macOS, main's arguments are taken as they are")
{
    char  lProgram[] = "Game";
    char  lFlag[]    = "--project";
    char  lValue[]   = "/home/Zo\xC3\xA9/P.opaaxproj";
    char* lArgv[]    = { lProgram, lFlag, lValue };

    const CommandLine lLine = CommandLine::FromMain(3, lArgv);

    REQUIRE(lLine.GetArguments().size() == 3u);
    CHECK(lLine.GetArguments()[0] == "Game");
    CHECK(lLine.GetValue("--project") == "/home/Zo\xC3\xA9/P.opaaxproj");
}
#endif

#ifdef OPAAX_PLATFORM_WINDOWS
TEST_CASE("CommandLine: on Windows, the wide command line in UTF-8, characters no code page holds included")
{
    // Two CJK characters in the capture's folder: argv would carry "??" for them on a Western Windows.
    const CommandLine lLine = CommandLine::FromWide(
        L"Game.exe --capture \"C:/My Shots/\x65E5\x672C/a.png\" --project C:/Zo\x00E9/P.opaaxproj");

    REQUIRE(lLine.GetArguments().size() == 5u);
    CHECK(lLine.GetArguments()[0] == "Game.exe");
    CHECK(lLine.GetValue("--capture") == "C:/My Shots/\xE6\x97\xA5\xE6\x9C\xAC/a.png");
    CHECK(lLine.GetValue("--project") == "C:/Zo\xC3\xA9/P.opaaxproj");
}

TEST_CASE("CommandLine: on Windows, the running program's arguments come from its wide command line")
{
    // main's argv is not read: the program is this test executable, whatever argv says.
    char  lFake[] = "Fake.exe";
    char* lArgv[] = { lFake };

    const CommandLine lLine = CommandLine::FromMain(1, lArgv);

    REQUIRE_FALSE(lLine.GetArguments().empty());
    CHECK(lLine.GetArguments()[0].Contains("OpaaxTests"));
}
#endif
