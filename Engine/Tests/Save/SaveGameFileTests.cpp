// Suite: the save file — a SaveGame written as text and read back, a file of an older class version
// upgraded with defaults for the new fields, newer files refused, and what is not a save file.
#include <doctest.h>

#include "Core/OpaaxTypes.h"
#include "Object/ObjectClassRegistry.h"
#include "Save/SaveGame.h"
#include "Save/SaveGameFile.h"

using namespace Opaax;

namespace SaveTestTypes
{
    struct TestLevelRecord
    {
        OpaaxString Level;
        float       BestTime = 0.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TestLevelRecord, Level, BestTime)

        OPAAX_PROPERTIES(TestLevelRecord, OPAAX_PROP(Level), OPAAX_PROP(BestTime))
    };

    // The save as the game ships it now. Version 2 added Coins and renamed "Runs" to RunCount.
    class TestProgressSave final : public SaveGame
    {
        OPAAX_OBJECT(TestProgressSave, SaveGame)

    public:
        TDynArray<TestLevelRecord> Records;
        Int32                      RunCount = 0;
        Int32                      Coins    = 10;

        /** Derived on load, never saved. */
        Int32 RecordCount = -1;

        OPAAX_PROPERTIES(TestProgressSave, OPAAX_PROP(Records), OPAAX_PROP(RunCount), OPAAX_PROP(Coins))

        Int32 GetSaveVersion() const override { return 2; }

        void UpgradeProperties(const Int32 InFileVersion, nlohmann::json& InOutProperties) const override
        {
            if (InFileVersion < 2 && InOutProperties.contains("Runs"))
            {
                InOutProperties["RunCount"] = InOutProperties["Runs"];
                InOutProperties.erase("Runs");
            }
        }

        void PostLoad() override { RecordCount = static_cast<Int32>(Records.size()); }
    };

    class TestSettingsSave final : public SaveGame
    {
        OPAAX_OBJECT(TestSettingsSave, SaveGame)

    public:
        float Volume = 1.f;
        OPAAX_PROPERTIES(TestSettingsSave, OPAAX_PROP(Volume))
    };

    // An Object that is not a SaveGame: a save file cannot hold one.
    class TestNotASave final : public Object
    {
        OPAAX_OBJECT(TestNotASave, Object)
    };
}

using namespace SaveTestTypes;

namespace
{
    /** The classes a game would register. */
    struct TestClasses
    {
        ObjectClassRegistry Registry;

        TestClasses()
        {
            Registry.Register<SaveGame>();
            Registry.Register<TestProgressSave>();
            Registry.Register<TestSettingsSave>();
            Registry.Register<TestNotASave>();
        }
    };

    const OpaaxStringID SLOT = OPAAX_ID("Slot1");
}

// =============================================================================
// Writing and reading back
// =============================================================================

TEST_CASE("SaveGameFile: class, versions and properties, keys sorted, as a person reads them")
{
    const TStrongObjectPtr<TestProgressSave> lSave = NewObject<TestProgressSave>();
    lSave->Records.push_back({ OpaaxString("Maps/One"), 12.5f });
    lSave->RunCount = 3;

    const OpaaxString lExpected =
        "{\n"
        "    \"Class\": \"TestProgressSave\",\n"
        "    \"ClassVersion\": 2,\n"
        "    \"Properties\": {\n"
        "        \"Coins\": 10,\n"
        "        \"Records\": [\n"
        "            {\n"
        "                \"BestTime\": 12.5,\n"
        "                \"Level\": \"Maps/One\"\n"
        "            }\n"
        "        ],\n"
        "        \"RunCount\": 3\n"
        "    },\n"
        "    \"Version\": 1\n"
        "}";
    CHECK(SaveGameFile::Serialize(*lSave) == lExpected);
}

TEST_CASE("SaveGameFile: a save round-trips")
{
    const TestClasses lClasses;

    const TStrongObjectPtr<TestProgressSave> lSaved = NewObject<TestProgressSave>();
    lSaved->Records.push_back({ OpaaxString("Maps/One"), 12.5f });
    lSaved->Records.push_back({ OpaaxString("Maps/Zo\xC3\xA9"), 40.25f });
    lSaved->RunCount = 7;
    lSaved->Coins    = 120;

    const OpaaxString lText = SaveGameFile::Serialize(*lSaved);

    TStrongObjectPtr<TestProgressSave> lLoaded;
    TDynArray<OpaaxString>             lBadFields;
    REQUIRE(SaveGameFile::Read(lText, lClasses.Registry, SLOT, lLoaded, nullptr, &lBadFields) == ESaveResult::Ok);
    REQUIRE(lLoaded.IsValid());

    CHECK(lBadFields.empty());
    CHECK(lLoaded->GetName() == SLOT);
    REQUIRE(lLoaded->Records.size() == 2);
    CHECK(lLoaded->Records[0].Level    == "Maps/One");
    CHECK(lLoaded->Records[0].BestTime == 12.5f);
    CHECK(lLoaded->Records[1].Level    == "Maps/Zo\xC3\xA9");
    CHECK(lLoaded->Records[1].BestTime == 40.25f);
    CHECK(lLoaded->RunCount == 7);
    CHECK(lLoaded->Coins    == 120);
    CHECK(lLoaded->RecordCount == 2);

    // Saving what was loaded writes the same text: nothing drifts from one launch to the next.
    CHECK(SaveGameFile::Serialize(*lLoaded) == lText);
}

TEST_CASE("SaveGameFile: a base class asked for reads the file's own class")
{
    const TestClasses lClasses;

    const TStrongObjectPtr<TestSettingsSave> lSaved = NewObject<TestSettingsSave>();
    lSaved->Volume = 0.25f;

    TStrongObjectPtr<SaveGame> lLoaded;
    REQUIRE(SaveGameFile::Read(SaveGameFile::Serialize(*lSaved), lClasses.Registry, SLOT, lLoaded) == ESaveResult::Ok);

    const TestSettingsSave* lSettings = Cast<TestSettingsSave>(lLoaded.Get());
    REQUIRE(lSettings != nullptr);
    CHECK(lSettings->Volume == 0.25f);
}

// =============================================================================
// Versions
// =============================================================================

TEST_CASE("SaveGameFile: an older file loads, upgraded, with defaults for the new fields")
{
    const TestClasses lClasses;

    // Version 1: no Coins yet, and the run count under its old name.
    const OpaaxString lVersion1(R"({
        "Class": "TestProgressSave",
        "ClassVersion": 1,
        "Properties": { "Records": [ { "Level": "Maps/One", "BestTime": 9.5 } ], "Runs": 4 },
        "Version": 1
    })");

    TStrongObjectPtr<TestProgressSave> lLoaded;
    TDynArray<OpaaxString>             lBadFields;
    REQUIRE(SaveGameFile::Read(lVersion1, lClasses.Registry, SLOT, lLoaded, nullptr, &lBadFields) == ESaveResult::Ok);

    CHECK(lBadFields.empty());
    CHECK(lLoaded->RunCount == 4);
    CHECK(lLoaded->Coins    == 10);
    REQUIRE(lLoaded->Records.size() == 1);
    CHECK(lLoaded->Records[0].BestTime == 9.5f);

    // Saved again, it is a version 2 file.
    SaveGameFile::Contents lContents;
    REQUIRE(SaveGameFile::Parse(SaveGameFile::Serialize(*lLoaded), lContents) == ESaveResult::Ok);
    CHECK(lContents.ClassVersion == 2);
    CHECK_FALSE(lContents.Properties.contains("Runs"));
}

TEST_CASE("SaveGameFile: a file written by hand may leave out the versions and the properties")
{
    const TestClasses lClasses;

    TStrongObjectPtr<TestProgressSave> lLoaded;
    REQUIRE(SaveGameFile::Read(OpaaxString(R"({ "Class": "TestProgressSave" })"), lClasses.Registry, SLOT, lLoaded)
            == ESaveResult::Ok);

    CHECK(lLoaded->Coins       == 10);
    CHECK(lLoaded->RecordCount == 0);
}

TEST_CASE("SaveGameFile: a file from a newer game or engine is refused")
{
    const TestClasses lClasses;

    TStrongObjectPtr<TestProgressSave> lLoaded;
    OpaaxString                        lError;

    SUBCASE("a newer class version")
    {
        const OpaaxString lText(R"({ "Class": "TestProgressSave", "ClassVersion": 3, "Properties": {}, "Version": 1 })");
        CHECK(SaveGameFile::Read(lText, lClasses.Registry, SLOT, lLoaded, &lError) == ESaveResult::NewerVersion);
        CHECK(lError.Contains("newer game"));
    }

    SUBCASE("a newer file format")
    {
        const OpaaxString lText(R"({ "Class": "TestProgressSave", "ClassVersion": 2, "Properties": {}, "Version": 2 })");
        CHECK(SaveGameFile::Read(lText, lClasses.Registry, SLOT, lLoaded, &lError) == ESaveResult::NewerVersion);
        CHECK(lError.Contains("newer engine"));
    }

    CHECK_FALSE(lLoaded.IsValid());
}

// =============================================================================
// Bad files
// =============================================================================

TEST_CASE("SaveGameFile: a value of the wrong type keeps its default, and the rest still loads")
{
    const TestClasses lClasses;

    const OpaaxString lText(R"({
        "Class": "TestProgressSave", "ClassVersion": 2, "Version": 1,
        "Properties": { "Coins": "lots", "RunCount": 5 }
    })");

    TStrongObjectPtr<TestProgressSave> lLoaded;
    TDynArray<OpaaxString>             lBadFields;
    REQUIRE(SaveGameFile::Read(lText, lClasses.Registry, SLOT, lLoaded, nullptr, &lBadFields) == ESaveResult::Ok);

    CHECK(lLoaded->RunCount == 5);
    CHECK(lLoaded->Coins    == 10);
    REQUIRE(lBadFields.size() == 1);
    CHECK(lBadFields[0] == "Coins");
}

TEST_CASE("SaveGameFile: what is not a save file is refused")
{
    const TestClasses lClasses;

    const TStrongObjectPtr<TestProgressSave> lSave = NewObject<TestProgressSave>();
    const OpaaxString lWhole = SaveGameFile::Serialize(*lSave);

    // A write that stopped halfway leaves the first half of the text.
    const OpaaxString lHalf = OpaaxStringView(lWhole).SubString(0, lWhole.GetLength() / 2).ToString();

    const char* const lBadTexts[] = {
        "",
        "not json",
        "[]",
        R"({ "Properties": {} })",
        R"({ "Class": "", "Properties": {} })",
        R"({ "Class": "TestProgressSave", "Properties": [] })",
        R"({ "Class": "TestProgressSave", "ClassVersion": "two" })",
        R"({ "Class": "TestProgressSave", "Version": 1.5 })",
    };

    for (const char* lBad : lBadTexts)
    {
        CAPTURE(lBad);
        TStrongObjectPtr<TestProgressSave> lLoaded;
        OpaaxString                        lError;
        CHECK(SaveGameFile::Read(OpaaxString(lBad), lClasses.Registry, SLOT, lLoaded, &lError) == ESaveResult::Corrupt);
        CHECK_FALSE(lError.IsEmpty());
        CHECK_FALSE(lLoaded.IsValid());
    }

    TStrongObjectPtr<TestProgressSave> lLoaded;
    CHECK(SaveGameFile::Read(lHalf, lClasses.Registry, SLOT, lLoaded) == ESaveResult::Corrupt);
    CHECK_FALSE(lLoaded.IsValid());
}

TEST_CASE("SaveGameFile: a file of another class is refused")
{
    const TestClasses lClasses;

    const char* const lOtherClasses[] = {
        R"({ "Class": "TestNoSuchSave" })",     // never registered (a class renamed without an alias)
        R"({ "Class": "TestSettingsSave" })",   // another save class
        R"({ "Class": "TestNotASave" })",       // an Object, not a save
        R"({ "Class": "SaveGame" })",           // abstract
    };

    for (const char* lText : lOtherClasses)
    {
        CAPTURE(lText);
        TStrongObjectPtr<TestProgressSave> lLoaded;
        OpaaxString                        lError;
        CHECK(SaveGameFile::Read(OpaaxString(lText), lClasses.Registry, SLOT, lLoaded, &lError) == ESaveResult::WrongClass);
        CHECK_FALSE(lError.IsEmpty());
        CHECK_FALSE(lLoaded.IsValid());
    }
}
