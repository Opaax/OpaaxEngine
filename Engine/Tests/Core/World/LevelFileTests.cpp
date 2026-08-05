// Suite: LevelFile — the `.opaaxlevel` reader, and the rule that NAMES A WORLD.
//
// The level's name is the world's name (WorldSpec carries no name of its own), so these cases
// pin where that name comes from: the `name` key when the author wrote one, the file's stem
// otherwise. A level that names itself keeps its name wherever the file is moved to, which is
// the whole reason the key beats mining the path.
//
// Runs against a UNIQUE directory under the OS temp dir, created and removed per case — the
// suite never touches the repo ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "World/Serialization/LevelFile.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxLevelFileTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        /** Write InText to InRel and return its absolute path, as LevelFile::Load takes one. */
        OpaaxString Write(const char* InRel, const char* InText) const
        {
            const fs::path lFull = m_Path / InRel;

            std::ofstream lOut(lFull, std::ios::binary);
            lOut << InText;
            lOut.close();

            return OpaaxString(lFull.generic_string().c_str());
        }

    private:
        fs::path m_Path;
    };
}

TEST_CASE("LevelFile: the 'name' key names the level")
{
    const ScopedTempDir lDir("named");
    const OpaaxString   lPath = lDir.Write("Main.opaaxlevel",
        R"({ "version": 1, "name": "Arena", "maps": [ "Maps/Main.opaaxmap" ] })");

    LevelData lData;
    REQUIRE(LevelFile::Load(lPath, lData));

    // The authored name wins over the stem — the file is called Main, the level is called Arena.
    CHECK(lData.Name == OpaaxString("Arena"));
    CHECK(lData.MapCount() == 1);
}

TEST_CASE("LevelFile: the file's stem is the fallback name")
{
    const ScopedTempDir lDir("unnamed");

    SUBCASE("no name key at all")
    {
        const OpaaxString lPath = lDir.Write("Main.opaaxlevel",
            R"({ "version": 1, "maps": [ "Maps/Main.opaaxmap" ] })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));
        CHECK(lData.Name == OpaaxString("Main"));
    }

    SUBCASE("an empty name is not a name")
    {
        const OpaaxString lPath = lDir.Write("Arena01.opaaxlevel",
            R"({ "version": 1, "name": "", "maps": [] })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));
        CHECK(lData.Name == OpaaxString("Arena01"));
    }

    SUBCASE("a non-string name is ignored, like every other mistyped field")
    {
        const OpaaxString lPath = lDir.Write("Boot.opaaxlevel",
            R"({ "version": 1, "name": 42, "maps": [] })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));
        CHECK(lData.Name == OpaaxString("Boot"));
    }

    SUBCASE("a stem with dots keeps everything up to the last one")
    {
        const OpaaxString lPath = lDir.Write("Main.v2.opaaxlevel",
            R"({ "version": 1, "maps": [] })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));
        CHECK(lData.Name == OpaaxString("Main.v2"));
    }
}

TEST_CASE("LevelFile: a successful load always yields a usable name")
{
    // The property the caller depends on: Engine::FinishStartup names the world from this and
    // has no fallback of its own once the load has succeeded.
    const ScopedTempDir lDir("always");
    const OpaaxString   lPath = lDir.Write("Empty.opaaxlevel", R"({ "version": 1 })");

    LevelData lData;
    REQUIRE(LevelFile::Load(lPath, lData));

    CHECK_FALSE(lData.Name.IsEmpty());
    CHECK(lData.IsEmpty());   // no maps is not a failure to read
}

TEST_CASE("LevelFile: a failed load leaves the caller's data untouched")
{
    const ScopedTempDir lDir("refused");

    LevelData lData;
    lData.Name = OpaaxString("Held");
    lData.Maps.push_back(OpaaxString("Maps/Held.opaaxmap"));

    SUBCASE("missing file")
    {
        CHECK_FALSE(LevelFile::Load(lDir.Write("Decoy.txt", ""), lData));
    }

    SUBCASE("not json")
    {
        CHECK_FALSE(LevelFile::Load(lDir.Write("Bad.opaaxlevel", "not json at all"), lData));
    }

    SUBCASE("a version newer than this build reads")
    {
        CHECK_FALSE(LevelFile::Load(
            lDir.Write("Future.opaaxlevel", R"({ "version": 99, "name": "Future" })"), lData));
    }

    CHECK(lData.Name == OpaaxString("Held"));
    CHECK(lData.MapCount() == 1);
}
