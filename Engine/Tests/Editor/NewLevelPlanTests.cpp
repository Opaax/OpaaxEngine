// Suite: NewLevelPlan — what New Level writes: a level of one map, named after its file, the map by
// default in Maps/ under the level's name. Text only: the editor test NewLevel.json writes the files.
#include <doctest.h>

#include "Editor/Operation/NewLevelPlan.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("NewLevelPlan: a new level's map is Maps/<level name>.opaaxmap unless one is named")
{
    CHECK(NewLevelPlan::DefaultMap(OpaaxString("Levels/Arena.opaaxlevel")) == "Maps/Arena.opaaxmap");
    CHECK(NewLevelPlan::DefaultMap(OpaaxString("Worlds/Deep/Cave 2.opaaxlevel")) == "Maps/Cave 2.opaaxmap");
}

TEST_CASE("NewLevelPlan: the level is named after its file and holds its one map, persistent")
{
    const LevelData lData = NewLevelPlan::Make(OpaaxString("Levels/Arena.opaaxlevel"), OpaaxString("Maps/Arena.opaaxmap"));

    CHECK(lData.Name == "Arena");
    REQUIRE(lData.MapCount() == 1);
    CHECK(lData.Maps[0] == "Maps/Arena.opaaxmap");
    CHECK(lData.PersistentMap() == "Maps/Arena.opaaxmap");

    // Written with its persistent map named.
    CHECK(LevelFile::Serialize(lData).Find("\"persistentMap\": \"Maps/Arena.opaaxmap\"") >= 0);
}

TEST_CASE("NewLevelPlan: a path is a level or a map only with its extension and a name")
{
    CHECK(NewLevelPlan::HasExtension(OpaaxString("Levels/Arena.opaaxlevel"), LevelFile::LEVEL_EXTENSION));
    CHECK(NewLevelPlan::HasExtension(OpaaxString("Maps/Arena.opaaxmap"), MapFile::MAP_EXTENSION));
    CHECK_FALSE(NewLevelPlan::HasExtension(OpaaxString("Levels/Arena.json"), LevelFile::LEVEL_EXTENSION));
    CHECK_FALSE(NewLevelPlan::HasExtension(OpaaxString("Levels/Arena"), LevelFile::LEVEL_EXTENSION));
    CHECK_FALSE(NewLevelPlan::HasExtension(OpaaxString("Levels/.opaaxlevel"), LevelFile::LEVEL_EXTENSION));
}
