// Suite: IFileSystem, through the concrete WindowsFileSystem (the interface is abstract — the whole
// point of the split). The facade was unreachable before M2d (private, non-const methods behind a
// const& accessor), so these are its first tests. They run against a UNIQUE directory under the OS
// temp dir — created, exercised and removed per case — so the suite never touches the repo and two
// runs never collide.
//
// GetPathIfNCreate is deliberately NOT exercised on its failure branch: that path resolves ILogger
// from the service locator, which this suite does not stand up.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Application/Services/Platforms/IFileSystem.h"
#include "Application/Services/Platforms/Windows/WindowsFileSystem.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // One temp directory per case, removed on scope exit — including when a CHECK throws.
    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxFileSystemTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);          // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        OpaaxString Sub(const char* InRel) const
        {
            return OpaaxString((m_Path / InRel).generic_string().c_str());
        }

        OpaaxString Str() const { return OpaaxString(m_Path.generic_string().c_str()); }

    private:
        fs::path m_Path;
    };

    void WriteFile(const OpaaxString& InAbs)
    {
        std::ofstream lFile(InAbs.CStr());
        lFile << "opaax";
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
    const ScopedTempDir lTemp("create_nested");
    const WindowsFileSystem lFS;

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
    const WindowsFileSystem lFS;
    const OpaaxString lEmpty;

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
    const ScopedTempDir lTemp("list_one_level");
    const WindowsFileSystem lFS;

    REQUIRE(lFS.CreateDirectories(lTemp.Sub("Waves")));
    WriteFile(lTemp.Sub("Waves/Deep.wave"));       // one level DOWN — must not appear
    WriteFile(lTemp.Sub("Root.txt"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Str(), lEntries));
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
    const ScopedTempDir lTemp("list_bad_targets");
    const WindowsFileSystem lFS;

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
    const ScopedTempDir lTemp("list_appends");
    const WindowsFileSystem lFS;

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
// Encoding — the reason WindowsFileSystem exists
// =============================================================================
// The platform layer emits UTF-8 by construction (WindowsPlatform::GetExecutablePath converts through
// CP_UTF8), so the filesystem must decode UTF-8 too. It did NOT before WindowsFileSystem: MSVC's
// std::filesystem::path(const char*) uses the ANSI code page, and this case caught it red —
// IsPathExist() answered false for a directory that plainly existed, and ListDirectory handed back
// CP-1252 bytes the caller would have stored as UTF-8.
//
// The directory is created from a WIDE literal, so the name on disk is unambiguously Unicode and the
// test cannot pass by being consistently wrong in both directions.
// =============================================================================
TEST_CASE("WindowsFileSystem: non-ASCII paths round-trip as UTF-8, not as the ANSI code page")
{
    const ScopedTempDir     lTemp("utf8_roundtrip");
    const WindowsFileSystem lFS;

    // \u escapes, NOT literal accented characters: this file has no BOM and the build sets no /utf-8,
    // so MSVC would decode literal bytes as the ANSI code page — the very confusion under test. A
    // universal-character-name means the same thing regardless of how the file is stored.
    const fs::path lDir = fs::path(lTemp.Str().CStr()) / std::wstring(L"\u00C9clair_\u00DCnicode");
    std::error_code lError;
    fs::create_directories(lDir, lError);
    REQUIRE_FALSE(lError);

    // The same name as UTF-8 BYTES — what the engine carries in an OpaaxString. Split escapes so the
    // hex does not swallow the following letter.
    const OpaaxString lUtf8Name("\xC3\x89" "clair_" "\xC3\x9C" "nicode");
    const OpaaxString lUtf8Dir(lTemp.Str() + OpaaxString("/") + lUtf8Name);

    CHECK(lFS.IsPathExist(lUtf8Dir));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Str(), lEntries));
    REQUIRE(lEntries.size() == 1);
    CHECK(lEntries[0].Name == lUtf8Name);
    CHECK(lEntries[0].bIsDirectory);

    // The listed path must be usable as an input — the round trip closes.
    CHECK(lFS.IsPathExist(lEntries[0].AbsPath));
}

TEST_CASE("WindowsFileSystem: CreateDirectories accepts a non-ASCII name and the OS agrees it is there")
{
    const ScopedTempDir     lTemp("utf8_create");
    const WindowsFileSystem lFS;

    const OpaaxString lUtf8Dir(lTemp.Str() + OpaaxString("/\xE6\x97\xA5\xE6\x9C\xAC" "_Waves"));
    REQUIRE(lFS.CreateDirectories(lUtf8Dir));

    // Ask the OS through the WIDE API, so a self-consistent mis-encoding cannot fake this.
    // U+65E5 U+672C — outside CP-1252 entirely, so this one cannot survive an ANSI round trip at all.
    const fs::path lExpected = fs::path(lTemp.Str().CStr()) / std::wstring(L"\u65E5\u672C_Waves");
    CHECK(fs::is_directory(lExpected));
}

// =============================================================================
// Null object
// =============================================================================
TEST_CASE("IFileSystem::Null: inert — every primitive fails and nothing reaches a disk")
{
    const ScopedTempDir lTemp("null_fs");
    const IFileSystem&  lNull = IFileSystem::Null();

    const OpaaxString lPath = lTemp.Sub("ShouldNeverAppear");

    CHECK_FALSE(lNull.CreateDirectories(lPath));
    CHECK_FALSE(fs::exists(fs::path(lPath.CStr())));   // it really did not create it
    CHECK_FALSE(lNull.IsPathExist(lTemp.Str()));       // false even though the dir DOES exist

    TDynArray<IFileSystem::Entry> lEntries;
    CHECK_FALSE(lNull.ListDirectory(lTemp.Str(), lEntries));
    CHECK(lEntries.empty());

    // The shared policy layer runs on top of the primitives, so it fails too — without throwing.
    CHECK(lNull.GetPathIfNCreate(OpaaxString()).IsEmpty());
}

TEST_CASE("IFileSystem: Entry paths use forward slashes and resolve back to the real file")
{
    const ScopedTempDir lTemp("entry_paths");
    const WindowsFileSystem lFS;

    WriteFile(lTemp.Sub("Wave01.wave"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Str(), lEntries));
    REQUIRE(lEntries.size() == 1);

    const IFileSystem::Entry& lEntry = lEntries[0];
    CHECK(lEntry.Name == "Wave01.wave");
    CHECK(lEntry.AbsPath.Find("\\") == -1);        // generic_string(): '/' on every platform
    CHECK(lFS.IsPathExist(lEntry.AbsPath));
}
