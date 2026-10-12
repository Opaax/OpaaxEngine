// Suite: FileSystem (Core/IO/FileSystem.h), the disk: questions, changes and the paths of the
// process. Each case works in its own temp directory.
#include <doctest.h>

#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"

#include "../../TestTempDir.h"

using namespace Opaax;

TEST_CASE("FileSystem: Exists, IsFile and IsDirectory tell a file from a directory")
{
    const TestTempDir lTemp("fs_kinds");
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("note.txt"), "opaax"));
    REQUIRE(FileSystem::CreateDirectories(lTemp.Sub("Maps")));

    CHECK(FileSystem::Exists(lTemp.Sub("note.txt")));
    CHECK(FileSystem::IsFile(lTemp.Sub("note.txt")));
    CHECK_FALSE(FileSystem::IsDirectory(lTemp.Sub("note.txt")));

    CHECK(FileSystem::IsDirectory(lTemp.Sub("Maps")));
    CHECK_FALSE(FileSystem::IsFile(lTemp.Sub("Maps")));

    CHECK_FALSE(FileSystem::Exists(lTemp.Sub("missing.txt")));
    CHECK_FALSE(FileSystem::Exists(OpaaxString()));
}

TEST_CASE("FileSystem: CreateDirectories, Remove and RemoveAll")
{
    const TestTempDir lTemp("fs_create_remove");

    CHECK(FileSystem::CreateDirectories(lTemp.Sub("A/B/C")));
    CHECK(FileSystem::CreateDirectories(lTemp.Sub("A/B/C")));   // already there: still true
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("A/B/C/leaf.txt"), "x"));

    // Remove: a file, or an empty directory only.
    CHECK_FALSE(FileSystem::Remove(lTemp.Sub("A/B")));
    CHECK(FileSystem::Remove(lTemp.Sub("A/B/C/leaf.txt")));
    CHECK_FALSE(FileSystem::Remove(lTemp.Sub("A/B/C/leaf.txt")));   // nothing left to remove
    CHECK(FileSystem::Remove(lTemp.Sub("A/B/C")));

    // RemoveAll: the tree; a missing path is not a failure.
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("A/B/deep.txt"), "x"));
    CHECK(FileSystem::RemoveAll(lTemp.Sub("A")));
    CHECK_FALSE(FileSystem::Exists(lTemp.Sub("A")));
    CHECK(FileSystem::RemoveAll(lTemp.Sub("A")));
}

TEST_CASE("FileSystem: Rename replaces the target, Copy only when asked to")
{
    const TestTempDir lTemp("fs_rename_copy");
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("a.txt"), "first"));
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("b.txt"), "second"));

    CHECK(FileSystem::Rename(lTemp.Sub("a.txt"), lTemp.Sub("b.txt")));
    CHECK_FALSE(FileSystem::Exists(lTemp.Sub("a.txt")));
    CHECK(FileIO::ReadAllText(lTemp.Sub("b.txt")) == "first");

    CHECK(FileSystem::Copy(lTemp.Sub("b.txt"), lTemp.Sub("c.txt")));
    CHECK(FileIO::ReadAllText(lTemp.Sub("c.txt")) == "first");

    REQUIRE(FileIO::WriteAllText(lTemp.Sub("d.txt"), "kept"));
    CHECK_FALSE(FileSystem::Copy(lTemp.Sub("b.txt"), lTemp.Sub("d.txt"), false));
    CHECK(FileIO::ReadAllText(lTemp.Sub("d.txt")) == "kept");
    CHECK(FileSystem::Copy(lTemp.Sub("b.txt"), lTemp.Sub("d.txt"), true));
    CHECK(FileIO::ReadAllText(lTemp.Sub("d.txt")) == "first");

    CHECK_FALSE(FileSystem::Rename(lTemp.Sub("missing.txt"), lTemp.Sub("e.txt")));
}

TEST_CASE("FileSystem: ListDirectory lists one level, appends, refuses a file")
{
    const TestTempDir lTemp("fs_list");
    REQUIRE(FileSystem::CreateDirectories(lTemp.Sub("Waves")));
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("Waves/Deep.wave"), "x"));
    REQUIRE(FileIO::WriteAllText(lTemp.Sub("Root.txt"), "x"));

    TDynArray<FileSystem::Entry> lEntries;
    REQUIRE(FileSystem::ListDirectory(lTemp.Path(), lEntries));
    REQUIRE(lEntries.size() == 2u);

    bool bSawWaves = false;
    bool bSawRoot  = false;
    for (const FileSystem::Entry& lEntry : lEntries)
    {
        if (lEntry.Name == "Waves") { bSawWaves = lEntry.bIsDirectory; }
        if (lEntry.Name == "Root.txt") { bSawRoot = !lEntry.bIsDirectory && FileSystem::IsFile(lEntry.AbsPath); }
        CHECK(lEntry.AbsPath.Find('\\') == -1);
    }
    CHECK(bSawWaves);
    CHECK(bSawRoot);

    CHECK(FileSystem::ListDirectory(lTemp.Sub("Waves"), lEntries));
    CHECK(lEntries.size() == 3u);   // appended

    CHECK_FALSE(FileSystem::ListDirectory(lTemp.Sub("Root.txt"), lEntries));
    CHECK(lEntries.size() == 3u);   // untouched
}

TEST_CASE("FileSystem: the working directory, the temp directory, absolute and canonical paths")
{
    const OpaaxString lWorking = FileSystem::WorkingDirectory();
    REQUIRE_FALSE(lWorking.IsEmpty());
    CHECK(FileSystem::IsDirectory(lWorking));
    CHECK(PathString::IsAbsolute(lWorking));

    CHECK(FileSystem::IsDirectory(FileSystem::TempDirectory()));

    // A relative path is taken from the working directory.
    CHECK(FileSystem::MakeAbsolute("Some/File.txt") == PathString::Join(lWorking, "Some/File.txt"));

    // Canonical: ".." resolved, and the part that does not exist yet kept as written.
    const TestTempDir lTemp("fs_canonical");
    REQUIRE(FileSystem::CreateDirectories(lTemp.Sub("A")));
    const OpaaxString lCanonical = FileSystem::MakeCanonical(lTemp.Sub("A/../A/new.txt"));
    CHECK(lCanonical.EndsWith("/A/new.txt"));
    CHECK_FALSE(lCanonical.Contains(".."));
    CHECK(FileSystem::MakeCanonical(lTemp.Sub("A/new.txt")) == lCanonical);
}
