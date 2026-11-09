// Suite: Process (Platform/Process.h), the programs the engine starts. Only what opens no window runs
// here: a folder shown in the file browser is checked by driving the editor's Open Save Folder.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Platform/Process.h"

using namespace Opaax;

TEST_CASE("Process: OpenFolder refuses what is not a folder, and starts nothing")
{
    const TestTempDir lTemp("process_open_folder");
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("Note.txt"), "x"));

    CHECK_FALSE(Process::OpenFolder(OpaaxString()));
    CHECK_FALSE(Process::OpenFolder(lTemp.Sub("Missing")));
    CHECK_FALSE(Process::OpenFolder(lTemp.Sub("Note.txt")));
}

TEST_CASE("Process: an environment variable is set, read back, changed and removed")
{
    const OpaaxString lName = "OPAAX_TEST_ENVIRONMENT";
    REQUIRE(Process::SetEnvironment(lName, OpaaxString()));
    CHECK_FALSE(Process::GetEnvironment(lName).has_value());

    REQUIRE(Process::SetEnvironment(lName, "480"));
    REQUIRE(Process::GetEnvironment(lName).has_value());
    CHECK(*Process::GetEnvironment(lName) == "480");

    // UTF-8 goes through the wide calls on Windows unchanged.
    REQUIRE(Process::SetEnvironment(lName, "caf\xC3\xA9 / 2"));
    CHECK(*Process::GetEnvironment(lName) == "caf\xC3\xA9 / 2");

    REQUIRE(Process::SetEnvironment(lName, OpaaxString()));
    CHECK_FALSE(Process::GetEnvironment(lName).has_value());
}

TEST_CASE("Process: a name that cannot be a variable is refused")
{
    CHECK_FALSE(Process::SetEnvironment(OpaaxString(), "x"));
    CHECK_FALSE(Process::SetEnvironment("A=B", "x"));
    CHECK_FALSE(Process::GetEnvironment(OpaaxString()).has_value());
}
