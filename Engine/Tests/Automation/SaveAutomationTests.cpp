// Suite: the save slot commands (save.list, save.get, save.set, save.clear) as functions of a save
// storage in a temp directory, with a game's save class registered.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Application/Services/ISaveStorage.h"
#include "Automation/SaveAutomationCommands.h"
#include "Object/ObjectClassRegistry.h"
#include "Save/SaveGame.h"

using namespace Opaax;

namespace SaveAutomationTestTypes
{
    class TestScoreSave final : public SaveGame
    {
        OPAAX_OBJECT(TestScoreSave, SaveGame)

    public:
        Int32       Score = 0;
        OpaaxString Name;

        OPAAX_PROPERTIES(TestScoreSave, OPAAX_PROP(Score), OPAAX_PROP(Name))
    };
}

using namespace SaveAutomationTestTypes;

namespace
{
    struct Fixture
    {
        TestTempDir         Temp{ "save_automation" };
        SaveStorage         Storage{ Temp.Sub("SaveGames") };
        ObjectClassRegistry Classes;

        Fixture()
        {
            Classes.Register<SaveGame>();
            Classes.Register<TestScoreSave>();
        }

        AutomationResult Set(const nlohmann::json& InParams) { return SaveAutomation::Set(Storage, Classes, InParams); }
    };
}

TEST_CASE("SaveAutomation: save.set writes a slot as the game writes it, and save.get reads it back")
{
    Fixture lFix;

    const AutomationResult lSet = lFix.Set({ { "slot", "Main" }, { "class", "TestScoreSave" },
                                             { "properties", { { "Score", 12 } } } });
    REQUIRE(lSet.bOk);
    CHECK(lSet.Value.at("path").get<OpaaxString>() == lFix.Storage.GetSlotPath("Main"));

    const AutomationResult lGet = SaveAutomation::Get(lFix.Storage, { { "slot", "Main" } });
    REQUIRE(lGet.bOk);
    CHECK(lGet.Value.at("slot") == "Main");
    CHECK(lGet.Value.at("class") == "TestScoreSave");
    CHECK(lGet.Value.at("classVersion") == 1);

    // The defaults are written with the values given.
    CHECK(lGet.Value.at("properties") == nlohmann::json{ { "Name", "" }, { "Score", 12 } });
}

TEST_CASE("SaveAutomation: save.set refuses an unknown class, an unknown property and a wrong type, writing nothing")
{
    Fixture lFix;

    CHECK_FALSE(lFix.Set({ { "slot", "Main" }, { "class", "NoSuchSave" } }).bOk);
    CHECK_FALSE(lFix.Set({ { "slot", "Main" }, { "class", "SaveGame" } }).bOk);
    CHECK_FALSE(lFix.Set({ { "slot", "Main" } }).bOk);
    CHECK_FALSE(lFix.Set({ { "slot", "../Main" }, { "class", "TestScoreSave" } }).bOk);

    // A misspelt property is named, with the ones the class has.
    const AutomationResult lTypo = lFix.Set({ { "slot", "Main" }, { "class", "TestScoreSave" },
                                              { "properties", { { "Scroe", 1 } } } });
    CHECK_FALSE(lTypo.bOk);
    CHECK(lTypo.Error.Contains("Scroe"));
    CHECK(lTypo.Error.Contains("Score"));

    CHECK_FALSE(lFix.Set({ { "slot", "Main" }, { "class", "TestScoreSave" },
                           { "properties", { { "Score", "many" } } } }).bOk);

    CHECK_FALSE(lFix.Storage.Exists("Main"));
}

TEST_CASE("SaveAutomation: save.set writes text as given, and save.get says when it is not a save")
{
    Fixture lFix;

    const OpaaxString lBroken("{ \"Class\": \"TestScoreSave\", \"Pro");
    REQUIRE(lFix.Set({ { "slot", "Broken" }, { "text", lBroken } }).bOk);

    OpaaxString lText;
    REQUIRE(lFix.Storage.Read("Broken", lText) == ESaveResult::Ok);
    CHECK(lText == lBroken);

    const AutomationResult lGet = SaveAutomation::Get(lFix.Storage, { { "slot", "Broken" } });
    CHECK_FALSE(lGet.bOk);
    CHECK(lGet.Error.Contains("does not hold a save"));

    CHECK_FALSE(SaveAutomation::Get(lFix.Storage, { { "slot", "Missing" } }).bOk);
    CHECK_FALSE(SaveAutomation::Get(lFix.Storage, nlohmann::json::object()).bOk);
}

TEST_CASE("SaveAutomation: save.list lists the slots and their folder; save.clear deletes one or all")
{
    Fixture lFix;
    REQUIRE(lFix.Storage.Write("B", "b") == ESaveResult::Ok);
    REQUIRE(lFix.Storage.Write("A", "a") == ESaveResult::Ok);
    REQUIRE(lFix.Storage.Write("C", "c") == ESaveResult::Ok);

    const AutomationResult lList = SaveAutomation::List(lFix.Storage);
    REQUIRE(lList.bOk);
    CHECK(lList.Value.at("folder").get<OpaaxString>() == lFix.Storage.GetSaveDirectory());
    CHECK(lList.Value.at("slots") == nlohmann::json{ "A", "B", "C" });

    // A slot without a save is no failure: a test clears its slots before it starts.
    CHECK(SaveAutomation::Clear(lFix.Storage, { { "slot", "B" } }).Value.at("deleted") == nlohmann::json{ "B" });
    CHECK(SaveAutomation::Clear(lFix.Storage, { { "slot", "B" } }).Value.at("deleted") == nlohmann::json::array());
    CHECK_FALSE(SaveAutomation::Clear(lFix.Storage, nlohmann::json::object()).bOk);

    CHECK(SaveAutomation::Clear(lFix.Storage, { { "all", true } }).Value.at("deleted") == nlohmann::json{ "A", "C" });
    CHECK(SaveAutomation::List(lFix.Storage).Value.at("slots") == nlohmann::json::array());
}
