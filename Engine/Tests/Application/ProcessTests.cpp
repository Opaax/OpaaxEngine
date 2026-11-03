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
