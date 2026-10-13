// Suite: IFileSystem, through NativeFileSystem. Each case uses its own directory under the OS
// temp dir, removed afterwards. Non-ASCII names: Utf8PathTests.cpp.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Platform/IFileSystem.h"
#include "Platform/NativeFileSystem.h"

using namespace Opaax;

namespace
{
    void WriteFile(const OpaaxString& InAbs)
    {
        FileIO::WriteAllText(InAbs, "opaax");
    }

    const IFileSystem::Entry* FindEntry(const TDynArray<IFileSystem::Entry>& InEntries, const char* InName)
    {
        for (const IFileSystem::Entry& lEntry : InEntries)
        {
            if (lEntry.Name == InName) { return &lEntry; }
        }
        return nullptr;
    }
}

// =============================================================================
// CreateDirectories / IsPathExist
// =============================================================================
TEST_CASE("IFileSystem: CreateDirectories builds a nested chain and is idempotent")
{
    const TestTempDir      lTemp("create_nested");
    const NativeFileSystem lFS{};

    const OpaaxString lNested = lTemp.Sub("A/B/C");

    CHECK_FALSE(lFS.IsPathExist(lNested));
    CHECK(lFS.CreateDirectories(lNested));
    CHECK(lFS.IsPathExist(lNested));

    // Second call creates nothing. std::filesystem reports false there; this contract reports true,
    // because "the directory exists" is what every caller actually asks.
    CHECK(lFS.CreateDirectories(lNested));
}

TEST_CASE("IFileSystem: an empty path is rejected by every entry point, never crashes")
{
    const NativeFileSystem lFS{};
    const OpaaxString lEmpty{};

    CHECK_FALSE(lFS.CreateDirectories(lEmpty));
    CHECK_FALSE(lFS.IsPathExist(lEmpty));
    CHECK(lFS.GetPathIfNCreate(lEmpty).IsEmpty());

    TDynArray<IFileSystem::Entry> lEntries;
    CHECK_FALSE(lFS.ListDirectory(lEmpty, lEntries));
    CHECK(lEntries.empty());
}

// =============================================================================
// ListDirectory
// =============================================================================
TEST_CASE("IFileSystem: ListDirectory separates files from directories, one level only")
{
    const TestTempDir      lTemp("list_one_level");
    const NativeFileSystem lFS{};

    REQUIRE(lFS.CreateDirectories(lTemp.Sub("Waves")));
    WriteFile(lTemp.Sub("Waves/Deep.wave"));       // one level DOWN — must not appear
    WriteFile(lTemp.Sub("Root.txt"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Path(), lEntries));
    CHECK(lEntries.size() == 2);

    const IFileSystem::Entry* lFolder = FindEntry(lEntries, "Waves");
    REQUIRE(lFolder != nullptr);
    CHECK(lFolder->bIsDirectory);

    const IFileSystem::Entry* lFile = FindEntry(lEntries, "Root.txt");
    REQUIRE(lFile != nullptr);
    CHECK_FALSE(lFile->bIsDirectory);

    // No recursion: the nested file is reachable only by listing the sub-directory itself.
    CHECK(FindEntry(lEntries, "Deep.wave") == nullptr);

    TDynArray<IFileSystem::Entry> lNested;
    REQUIRE(lFS.ListDirectory(lFolder->AbsPath, lNested));
    CHECK(lNested.size() == 1);
    CHECK(FindEntry(lNested, "Deep.wave") != nullptr);
}

TEST_CASE("IFileSystem: ListDirectory reports false for a missing dir and for a file, leaving the output untouched")
{
    const TestTempDir      lTemp("list_bad_targets");
    const NativeFileSystem lFS{};

    WriteFile(lTemp.Sub("NotADir.txt"));

    TDynArray<IFileSystem::Entry> lEntries;
    lEntries.push_back(IFileSystem::Entry{ OpaaxString("Sentinel"), OpaaxString(), false });

    CHECK_FALSE(lFS.ListDirectory(lTemp.Sub("DoesNotExist"), lEntries));
    CHECK_FALSE(lFS.ListDirectory(lTemp.Sub("NotADir.txt"), lEntries));

    // A rejected listing must not disturb what the caller already accumulated.
    CHECK(lEntries.size() == 1);
    CHECK(lEntries[0].Name == "Sentinel");
}

TEST_CASE("IFileSystem: ListDirectory APPENDS, so one container can accumulate several roots")
{
    const TestTempDir      lTemp("list_appends");
    const NativeFileSystem lFS{};

    REQUIRE(lFS.CreateDirectories(lTemp.Sub("RootA")));
    REQUIRE(lFS.CreateDirectories(lTemp.Sub("RootB")));
    WriteFile(lTemp.Sub("RootA/a.txt"));
    WriteFile(lTemp.Sub("RootB/b.txt"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Sub("RootA"), lEntries));
    REQUIRE(lFS.ListDirectory(lTemp.Sub("RootB"), lEntries));

    CHECK(lEntries.size() == 2);
    CHECK(FindEntry(lEntries, "a.txt") != nullptr);
    CHECK(FindEntry(lEntries, "b.txt") != nullptr);
}

// =============================================================================
// Null object
// =============================================================================
TEST_CASE("IFileSystem::Null: inert — every primitive fails and nothing reaches a disk")
{
    const TestTempDir   lTemp("null_fs");
    const IFileSystem&  lNull = IFileSystem::Null();

    const OpaaxString lPath = lTemp.Sub("ShouldNeverAppear");

    CHECK_FALSE(lNull.CreateDirectories(lPath));
    CHECK_FALSE(FileSystem::Exists(lPath));            // it really did not create it
    CHECK_FALSE(lNull.IsPathExist(lTemp.Path()));      // false even though the dir DOES exist

    TDynArray<IFileSystem::Entry> lEntries;
    CHECK_FALSE(lNull.ListDirectory(lTemp.Path(), lEntries));
    CHECK(lEntries.empty());

    // The shared policy layer runs on top of the primitives, so it fails too — without throwing.
    CHECK(lNull.GetPathIfNCreate(OpaaxString()).IsEmpty());
}

TEST_CASE("IFileSystem: Entry paths use forward slashes and resolve back to the real file")
{
    const TestTempDir      lTemp("entry_paths");
    const NativeFileSystem lFS{};

    WriteFile(lTemp.Sub("Wave01.wave"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Path(), lEntries));
    REQUIRE(lEntries.size() == 1);

    const IFileSystem::Entry& lEntry = lEntries[0];
    CHECK(lEntry.Name == "Wave01.wave");
    CHECK(lEntry.AbsPath.Find("\\") == -1);        // generic_string(): '/' on every platform
    CHECK(lFS.IsPathExist(lEntry.AbsPath));
}

TEST_CASE("IFileSystem: GetPathIfNCreate answers empty when the directories cannot be created")
{
    // The failure branch logs; the Logger singleton holds the line (no Init here), nothing crashes.
    CHECK(IFileSystem::Null().GetPathIfNCreate(OpaaxString("W:/never/created")).IsEmpty());
    CHECK(IFileSystem::Null().GetPathIfNCreate(OpaaxString()).IsEmpty());
}
