// Suite: IFileSystem. The facade was unreachable before M2d (private, non-const methods behind a
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
    const IFileSystem   lFS;

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
    const IFileSystem lFS;
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
    const IFileSystem   lFS;

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
    const IFileSystem   lFS;

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
    const IFileSystem   lFS;

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

TEST_CASE("IFileSystem: Entry paths use forward slashes and resolve back to the real file")
{
    const ScopedTempDir lTemp("entry_paths");
    const IFileSystem   lFS;

    WriteFile(lTemp.Sub("Wave01.wave"));

    TDynArray<IFileSystem::Entry> lEntries;
    REQUIRE(lFS.ListDirectory(lTemp.Str(), lEntries));
    REQUIRE(lEntries.size() == 1);

    const IFileSystem::Entry& lEntry = lEntries[0];
    CHECK(lEntry.Name == "Wave01.wave");
    CHECK(lEntry.AbsPath.Find("\\") == -1);        // generic_string(): '/' on every platform
    CHECK(lFS.IsPathExist(lEntry.AbsPath));
}
