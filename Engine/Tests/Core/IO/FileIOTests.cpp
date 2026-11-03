// Suite: FileIO's atomic writes (Core/IO/FileIO.h): a file written whole or not at all, so a crash or
// a failed write never leaves a player's save cut in half. Each case works in its own temp directory.
#include <doctest.h>

#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxStringView.hpp"

#include "../../TestTempDir.h"

using namespace Opaax;

TEST_CASE("FileIO: an atomic write makes the file and its folders, and leaves no temporary file")
{
    const TestTempDir lTemp("fileio_atomic_new");
    const OpaaxString lPath = lTemp.Sub("SaveGames/Slot1.opaaxsave");

    REQUIRE(FileIO::WriteAllTextAtomic(lPath, "{ \"Coins\": 3 }\n"));

    CHECK(FileIO::ReadAllText(lPath) == "{ \"Coins\": 3 }\n");
    CHECK_FALSE(FileSystem::Exists(FileIO::AtomicTempPath(lPath)));

    // Nothing to write is an empty file, not a missing one.
    REQUIRE(FileIO::WriteAllTextAtomic(lTemp.Sub("Empty.txt"), OpaaxString()));
    CHECK(FileSystem::IsFile(lTemp.Sub("Empty.txt")));
    CHECK(FileIO::ReadAllText(lTemp.Sub("Empty.txt")).IsEmpty());
}

TEST_CASE("FileIO: an atomic write replaces the old file whole, longer or shorter")
{
    const TestTempDir lTemp("fileio_atomic_replace");
    const OpaaxString lPath = lTemp.Sub("Slot1.opaaxsave");

    REQUIRE(FileIO::WriteAllTextAtomic(lPath, "short"));
    REQUIRE(FileIO::WriteAllTextAtomic(lPath, "a much longer text than before"));
    CHECK(FileIO::ReadAllText(lPath) == "a much longer text than before");

    REQUIRE(FileIO::WriteAllTextAtomic(lPath, "tiny"));
    CHECK(FileIO::ReadAllText(lPath) == "tiny");
    CHECK_FALSE(FileSystem::Exists(FileIO::AtomicTempPath(lPath)));
}

TEST_CASE("FileIO: a write that stopped halfway never replaces a good file")
{
    const TestTempDir lTemp("fileio_atomic_crash");
    const OpaaxString lPath = lTemp.Sub("Slot1.opaaxsave");
    const OpaaxString lGood("{ \"Coins\": 3 }");
    const OpaaxString lNext("{ \"Coins\": 4, \"RunCount\": 12 }");

    REQUIRE(FileIO::WriteAllTextAtomic(lPath, lGood));

    // What a crash in the middle of the next write leaves: its first half, in the temporary file.
    const OpaaxString lHalf = OpaaxStringView(lNext).SubString(0, lNext.GetLength() / 2).ToString();
    REQUIRE(FileIO::WriteAllText(FileIO::AtomicTempPath(lPath), lHalf));

    CHECK(FileIO::ReadAllText(lPath) == lGood);

    // The next launch writes over what the crash left.
    REQUIRE(FileIO::WriteAllTextAtomic(lPath, lNext));
    CHECK(FileIO::ReadAllText(lPath) == lNext);
    CHECK_FALSE(FileSystem::Exists(FileIO::AtomicTempPath(lPath)));
}

TEST_CASE("FileIO: a failed atomic write leaves what was there, and no temporary file of its own")
{
    const TestTempDir lTemp("fileio_atomic_fail");
    const OpaaxString lPath = lTemp.Sub("Slot1.opaaxsave");

    SUBCASE("the temporary file cannot be made")
    {
        REQUIRE(FileIO::WriteAllTextAtomic(lPath, "good"));

        // A folder where the temporary file goes: the write fails before the old file is touched.
        REQUIRE(FileSystem::CreateDirectories(FileIO::AtomicTempPath(lPath)));

        CHECK_FALSE(FileIO::WriteAllTextAtomic(lPath, "next"));
        CHECK(FileIO::ReadAllText(lPath) == "good");

        // What the write did not make, it leaves.
        CHECK(FileSystem::IsDirectory(FileIO::AtomicTempPath(lPath)));
    }

    SUBCASE("the data is written but cannot replace the target")
    {
        // A folder holding a file where the save goes: no file can be renamed over it.
        REQUIRE(FileIO::WriteAllText(lTemp.Sub("Slot1.opaaxsave/Keep.txt"), "kept"));

        CHECK_FALSE(FileIO::WriteAllTextAtomic(lPath, "next"));
        CHECK(FileSystem::IsDirectory(lPath));
        CHECK(FileIO::ReadAllText(lTemp.Sub("Slot1.opaaxsave/Keep.txt")) == "kept");
        CHECK_FALSE(FileSystem::Exists(FileIO::AtomicTempPath(lPath)));
    }

    SUBCASE("no path")
    {
        CHECK_FALSE(FileIO::WriteAllTextAtomic(OpaaxString(), "next"));
        CHECK_FALSE(FileIO::WriteAllBytesAtomic(lPath, nullptr, 4));
        CHECK_FALSE(FileSystem::Exists(lPath));
    }
}
