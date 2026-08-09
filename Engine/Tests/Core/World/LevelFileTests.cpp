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

TEST_CASE("LevelFile: 'persistentMap' resolves to an index into the level's own maps")
{
    // WM1a: the always-mounted map is named by PATH in the file and held as an INDEX in memory,
    // so "it is one of this level's maps" is decided once, here, and never re-checked.
    const ScopedTempDir lDir("persistent");

    SUBCASE("a named map wins, wherever it sits in the list")
    {
        const OpaaxString lPath = lDir.Write("Main.opaaxlevel", R"({
            "version": 1,
            "maps": [ "Maps/A.opaaxmap", "Maps/B.opaaxmap", "Maps/C.opaaxmap" ],
            "persistentMap": "Maps/B.opaaxmap" })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));

        CHECK(lData.PersistentMapIndex == 1);
        CHECK(lData.PersistentMap() == OpaaxString("Maps/B.opaaxmap"));
    }

    SUBCASE("absent defaults to the first entry — no existing manifest becomes invalid")
    {
        const OpaaxString lPath = lDir.Write("NoKey.opaaxlevel",
            R"({ "version": 1, "maps": [ "Maps/A.opaaxmap", "Maps/B.opaaxmap" ] })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));

        CHECK(lData.PersistentMapIndex == 0);
        CHECK(lData.PersistentMap() == OpaaxString("Maps/A.opaaxmap"));
    }

    SUBCASE("a name matching no entry defaults, and the level still LOADS")
    {
        // Tolerant and total (MP3): the author is warned, not refused. A level that stopped
        // opening over a mistyped optional field would cost far more than the field is worth.
        const OpaaxString lPath = lDir.Write("Ghost.opaaxlevel", R"({
            "version": 1,
            "maps": [ "Maps/A.opaaxmap", "Maps/B.opaaxmap" ],
            "persistentMap": "Maps/Nowhere.opaaxmap" })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));

        CHECK(lData.PersistentMapIndex == 0);
        CHECK(lData.MapCount() == 2);
    }

    SUBCASE("a non-string is ignored, like every other mistyped field")
    {
        const OpaaxString lPath = lDir.Write("Typed.opaaxlevel", R"({
            "version": 1,
            "maps": [ "Maps/A.opaaxmap", "Maps/B.opaaxmap" ],
            "persistentMap": 7 })");

        LevelData lData;
        REQUIRE(LevelFile::Load(lPath, lData));
        CHECK(lData.PersistentMapIndex == 0);
    }
}

TEST_CASE("LevelFile: an empty level has no persistent map to hand out")
{
    // PersistentMap() is TOTAL rather than merely documented: it is reached from the editor and
    // from Level::MountAll, and an empty level is a state both of them can be in.
    const ScopedTempDir lDir("empty-persistent");
    const OpaaxString   lPath = lDir.Write("Empty.opaaxlevel",
        R"({ "version": 1, "maps": [], "persistentMap": "Maps/A.opaaxmap" })");

    LevelData lData;
    REQUIRE(LevelFile::Load(lPath, lData));

    CHECK(lData.IsEmpty());
    CHECK(lData.PersistentMap().IsEmpty());
}

TEST_CASE("LevelFile: Save -> Load is a fixed point")
{
    // The writer half (MP4). A manifest the editor can author is only useful if reading back what
    // it wrote gives the same level — including the persistent map, which is the one field that
    // could quietly degrade to "the first entry" without anyone noticing.
    const ScopedTempDir lDir("save");
    const OpaaxString   lPath = lDir.Write("Written.opaaxlevel", "");

    LevelData lWritten;
    lWritten.Name = OpaaxString("Arena");
    lWritten.Maps.push_back(OpaaxString("Maps/A.opaaxmap"));
    lWritten.Maps.push_back(OpaaxString("Maps/B.opaaxmap"));
    lWritten.PersistentMapIndex = 1;

    REQUIRE(LevelFile::Save(lPath, lWritten));

    LevelData lRead;
    REQUIRE(LevelFile::Load(lPath, lRead));

    CHECK(lRead.Name == lWritten.Name);
    CHECK(lRead.MapCount() == 2);
    CHECK(lRead.PersistentMapIndex == 1);
    CHECK(lRead.PersistentMap() == OpaaxString("Maps/B.opaaxmap"));

    // And the text is STABLE, which is what the editor's dirty check rests on (MP5): a second
    // Save of what was just read must not report the level as changed.
    CHECK(LevelFile::Serialize(lRead) == LevelFile::Serialize(lWritten));
}
