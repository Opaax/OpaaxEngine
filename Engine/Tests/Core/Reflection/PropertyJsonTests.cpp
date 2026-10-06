// Suite: PropertyJson — a reflected type's JSON from its property list alone.
#include <doctest.h>

#include "Engine/Reflection/PropertyJson.h"
#include "Resources/DataAsset/DataAssetRef.h"
#include "Renderer/Components/QuadComponent.h"

using namespace Opaax;

namespace
{
    struct JsonTestResource {};

    // Outside Opaax on purpose, like a game's enum.
    enum class EJsonTestMode : Uint8 { Idle, Walk, Run };

    const char* ToString(const EJsonTestMode InMode) noexcept
    {
        switch (InMode)
        {
        case EJsonTestMode::Idle: return "Idle";
        case EJsonTestMode::Walk: return "Walk";
        case EJsonTestMode::Run:  return "Run";
        }
        return "?";
    }
}

OPAAX_ENUM_VALUES(EJsonTestMode, Idle, Walk, Run)

namespace
{
    struct JsonTestData
    {
        float Value = 0.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(JsonTestData, Value)
        OPAAX_PROPERTIES(JsonTestData, OPAAX_PROP(Value))
    };

    struct JsonTestInner
    {
        float Amount = 1.f;
        Int32 Level  = 2;

        OPAAX_PROPERTIES(JsonTestInner, OPAAX_PROP(Amount), OPAAX_PROP(Level))
    };

    // No NLOHMANN macro: the property list is the only description.
    struct JsonTestSubject
    {
        bool                            bEnabled = true;
        Int32                           Count    = 3;
        float                           Speed    = 1.5f;
        Vector2F                        Offset   = { 1.f, 2.f };
        LinearColor                     Tint;
        OpaaxString                     Label    = OpaaxString("hello");
        OpaaxStringID                   Id;
        OpaaxTag                        Team;
        EJsonTestMode                   Mode     = EJsonTestMode::Walk;
        TResourcePath<JsonTestResource> Asset;
        TDataAssetRef<JsonTestData>     Stats;
        JsonTestInner                   Inner;
        TDynArray<Int32>                Numbers;
        Int32                           NotListed = 7;

        OPAAX_PROPERTIES(JsonTestSubject,
                         OPAAX_PROP(bEnabled),
                         OPAAX_PROP(Count),
                         OPAAX_PROP(Speed),
                         OPAAX_PROP(Offset),
                         OPAAX_PROP(Tint),
                         OPAAX_PROP(Label),
                         OPAAX_PROP(Id),
                         OPAAX_PROP(Team),
                         OPAAX_PROP(Mode),
                         OPAAX_PROP(Asset),
                         OPAAX_PROP(Stats),
                         OPAAX_PROP(Inner),
                         OPAAX_PROP(Numbers))
    };

    JsonTestSubject MakeEdited()
    {
        JsonTestSubject lSubject;
        lSubject.bEnabled     = false;
        lSubject.Count        = -12;
        lSubject.Speed        = 0.25f;
        lSubject.Offset       = { -3.5f, 8.f };
        lSubject.Tint         = { 0.5f, 0.25f, 1.f, 0.75f };
        lSubject.Label        = OpaaxString("edited");
        lSubject.Id           = OpaaxStringID("Json.Test.Id");
        lSubject.Team         = OpaaxTag("Team.Red");
        lSubject.Mode         = EJsonTestMode::Run;
        lSubject.Asset.Path   = OpaaxString("Textures/Hero.png");
        lSubject.Stats.Path   = OpaaxString("Data/Grunt.opaaxdata");
        lSubject.Inner.Amount = 4.5f;
        lSubject.Inner.Level  = 9;
        lSubject.Numbers      = { 1, 2, 3 };
        lSubject.NotListed    = 99;
        return lSubject;
    }
}

TEST_CASE("PropertyJson: every listed field round-trips, with no NLOHMANN macro")
{
    const JsonTestSubject lEdited = MakeEdited();
    const nlohmann::json  lJson   = PropertiesToJson(lEdited);

    JsonTestSubject        lRead;
    TDynArray<OpaaxString> lBad;
    CHECK(PropertiesFromJson(lJson, lRead, &lBad));
    CHECK(lBad.empty());

    CHECK(PropertiesToJson(lRead) == lJson);
    CHECK(lRead.bEnabled == false);
    CHECK(lRead.Count == -12);
    CHECK(lRead.Mode == EJsonTestMode::Run);
    CHECK(lRead.Team == OpaaxTag("Team.Red"));
    CHECK(lRead.Asset.Path == "Textures/Hero.png");
    CHECK(lRead.Stats.Path == "Data/Grunt.opaaxdata");
    CHECK(lRead.Inner.Level == 9);
    CHECK(lRead.Numbers == TDynArray<Int32>{ 1, 2, 3 });
}

TEST_CASE("PropertyJson: the file shape: field names as keys, a group as an object, an enum as its label")
{
    const nlohmann::json lJson = PropertiesToJson(MakeEdited());

    CHECK(lJson.size() == 13);              // the listed fields only
    CHECK_FALSE(lJson.contains("NotListed"));
    CHECK(lJson.at("Mode") == "Run");       // a label, so reordering the enum is safe
    CHECK(lJson.at("Asset") == "Textures/Hero.png");
    CHECK(lJson.at("Inner").is_object());
    CHECK(lJson.at("Inner").at("Level") == 9);
}

TEST_CASE("PropertyJson: a type with both descriptions writes exactly what its NLOHMANN macro writes")
{
    QuadComponent lQuad;
    lQuad.Size  = { 12.f, 34.f };
    lQuad.Color = { 0.5f, 0.25f, 1.f, 1.f };

    // Same bytes, so a type can move from the macro to its property list with no file change.
    CHECK(PropertiesToJson(lQuad).dump() == nlohmann::json(lQuad).dump());
}

TEST_CASE("PropertyJson: a missing key keeps the field's value")
{
    JsonTestSubject        lRead;
    TDynArray<OpaaxString> lBad;
    CHECK(PropertiesFromJson(nlohmann::json{ { "Count", 7 }, { "Inner", { { "Level", 5 } } } }, lRead, &lBad));

    CHECK(lBad.empty());
    CHECK(lRead.Count == 7);
    CHECK(lRead.Inner.Level == 5);
    CHECK(lRead.Inner.Amount == 1.f);       // missing inside the group too
    CHECK(lRead.Speed == 1.5f);
    CHECK(lRead.Label == "hello");
    CHECK(lRead.Mode == EJsonTestMode::Walk);
}

TEST_CASE("PropertyJson: a wrong-typed value keeps the field's value and names the field")
{
    const nlohmann::json lJson = {
        { "bEnabled", false },                       // good, still read
        { "Count",    "seven" },
        { "Offset",   { { "x", 9.f } } },            // "y" missing: must not half-write x
        { "Label",    5 },
        { "Mode",     "Sprint" },                    // not a label of the enum
        { "Inner",    { { "Amount", "lots" }, { "Level", 6 } } },
        { "Numbers",  "not a list" },
    };

    JsonTestSubject        lRead;
    TDynArray<OpaaxString> lBad;
    CHECK(PropertiesFromJson(lJson, lRead, &lBad));

    CHECK(lRead.bEnabled == false);
    CHECK(lRead.Count == 3);
    CHECK(lRead.Offset == Vector2F{ 1.f, 2.f });
    CHECK(lRead.Label == "hello");
    CHECK(lRead.Mode == EJsonTestMode::Walk);
    CHECK(lRead.Inner.Amount == 1.f);
    CHECK(lRead.Inner.Level == 6);              // its sibling in the group is still read
    CHECK(lRead.Numbers.empty());

    // In declaration order, nested names with a dot.
    REQUIRE(lBad.size() == 6);
    CHECK(lBad[0] == "Count");
    CHECK(lBad[1] == "Offset");
    CHECK(lBad[2] == "Label");
    CHECK(lBad[3] == "Mode");
    CHECK(lBad[4] == "Inner.Amount");
    CHECK(lBad[5] == "Numbers");
}

TEST_CASE("PropertyJson: not an object reads nothing")
{
    JsonTestSubject        lRead;
    TDynArray<OpaaxString> lBad;

    CHECK_FALSE(PropertiesFromJson(nlohmann::json(5), lRead, &lBad));
    CHECK(lRead.Count == 3);

    // A group that is not an object is one bad field, its fields keep their values.
    CHECK(PropertiesFromJson(nlohmann::json{ { "Inner", 5 } }, lRead, &lBad));
    CHECK(lRead.Inner.Amount == 1.f);
    REQUIRE(lBad.size() == 1);
    CHECK(lBad[0] == "Inner");

    // The out list is optional.
    CHECK(PropertiesFromJson(nlohmann::json{ { "Count", "x" } }, lRead));
    CHECK(lRead.Count == 3);
}
