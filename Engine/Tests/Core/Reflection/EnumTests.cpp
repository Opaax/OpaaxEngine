// Suite: OPAAX_ENUM_VALUES and the generic enum JSON bridge. An enum is saved as its label,
// never its number, so inserting a value does not change what saved files mean.
#include <doctest.h>

#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Platform/Window/Window.h"
#include "RHI/RHIBackend.h"

// An enum declared the way a game declares one: in its own namespace, never in Opaax.
namespace EnumTestsGame
{
    enum class ERunZone { Start, Checkpoint, Finish };

    inline const char* ToString(const ERunZone InZone) noexcept
    {
        switch (InZone)
        {
        case ERunZone::Start:      return "Start";
        case ERunZone::Checkpoint: return "Checkpoint";
        case ERunZone::Finish:     return "Finish";
        }

        return "Start";
    }
}

OPAAX_ENUM_VALUES(EnumTestsGame::ERunZone, Start, Checkpoint, Finish)

namespace EnumTestsGame
{
    // Saved by nlohmann's macro alone, as a game's component is.
    struct RunRecord
    {
        ERunZone     Zone = ERunZone::Start;
        Opaax::Int32 Laps = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RunRecord, Zone, Laps)
    };
}

using namespace Opaax;
using EnumTestsGame::ERunZone;
using EnumTestsGame::RunRecord;

namespace
{
    enum class ENoValues { A, B };
}

TEST_CASE("OPAAX_ENUM_VALUES: the list is a constant expression, and it is the enum's own")
{
    static_assert(EnumValueCount<EWindowMode>() == 3);
    static_assert(EnumValueCount<EBackend>() == 2);

    static_assert(TEnumValues<EWindowMode>::Values[0] == EWindowMode::Windowed);
    static_assert(TEnumValues<EWindowMode>::Values[2] == EWindowMode::Fullscreen);

    // An enum nobody described is not drawable and not serializable — a compile error at the point of
    // use, never an empty dropdown.
    static_assert(CEnumWithValues<EWindowMode>);
    static_assert(!CEnumWithValues<ENoValues>);

    CHECK(true); // the assertions above are the test
}

TEST_CASE("Enum json: written as the ToString LABEL, not as an ordinal")
{
    // The claim the config format rests on. An ordinal would make inserting an enumerator silently
    // re-point every value already saved.
    const nlohmann::json lJson = nlohmann::json(EWindowMode::Borderless);

    REQUIRE(lJson.is_string());
    CHECK(lJson.get<OpaaxString>() == "Borderless");

    // And it is the SAME text the string-typed config field used to hold, which is why converting it
    // left Engine.config byte-identical.
    CHECK(nlohmann::json(EBackend::OpenGL).get<OpaaxString>() == "OpenGL");
}

TEST_CASE("Enum json: every declared value round-trips")
{
    for (const EWindowMode lMode : TEnumValues<EWindowMode>::Values)
    {
        EWindowMode lBack{};
        nlohmann::json(lMode).get_to(lBack);

        CHECK(lBack == lMode);
    }

    for (const EBackend lBackend : TEnumValues<EBackend>::Values)
    {
        EBackend lBack{};
        nlohmann::json(lBackend).get_to(lBack);

        CHECK(lBack == lBackend);
    }
}

TEST_CASE("Enum json: an unknown label THROWS rather than picking something")
{
    EWindowMode lMode = EWindowMode::Fullscreen;

    // An unknown label throws (TConfig::Load catches it and ConfigSystem warns).
    CHECK_THROWS_AS(nlohmann::json("Maximized").get_to(lMode), nlohmann::json::exception);
    CHECK_THROWS_AS(nlohmann::json("").get_to(lMode), nlohmann::json::exception);

    // Wrong TYPE is refused too — a number is not a label, even though the enum has ordinals.
    CHECK_THROWS_AS(nlohmann::json(1).get_to(lMode), nlohmann::json::exception);

    // Nothing was half-applied.
    CHECK(lMode == EWindowMode::Fullscreen);
}

TEST_CASE("Enum json: an enum outside Opaax is saved as its label by nlohmann's struct macro")
{
    // nlohmann alone would find no to_json in EnumTestsGame and write the number 2.
    const nlohmann::json lJson = RunRecord{ ERunZone::Finish, 3 };

    REQUIRE(lJson.at("Zone").is_string());
    CHECK(lJson.at("Zone").get<OpaaxString>() == "Finish");

    const RunRecord lBack = lJson.get<RunRecord>();
    CHECK(lBack.Zone == ERunZone::Finish);
    CHECK(lBack.Laps == 3);

    // Every value, bare.
    for (const ERunZone lZone : TEnumValues<ERunZone>::Values)
    {
        ERunZone lRead = ERunZone::Start;
        nlohmann::json(lZone).get_to(lRead);
        CHECK(lRead == lZone);
    }
}

TEST_CASE("Enum json: an unknown label of an enum outside Opaax throws")
{
    const nlohmann::json lJson = { { "Zone", "Lobby" }, { "Laps", 1 } };
    CHECK_THROWS_AS(lJson.get<RunRecord>(), nlohmann::json::exception);

    ERunZone lZone = ERunZone::Checkpoint;
    CHECK_THROWS_AS(nlohmann::json("Lobby").get_to(lZone), nlohmann::json::exception);
    CHECK(lZone == ERunZone::Checkpoint);
}

TEST_CASE("Enum json: Opaax::to_json and from_json called by name match what nlohmann writes")
{
    nlohmann::json lEngine;
    ::Opaax::to_json(lEngine, EWindowMode::Borderless);
    CHECK(lEngine == nlohmann::json(EWindowMode::Borderless));

    nlohmann::json lGame;
    ::Opaax::to_json(lGame, ERunZone::Checkpoint);
    CHECK(lGame == nlohmann::json(ERunZone::Checkpoint));

    EWindowMode lMode = EWindowMode::Windowed;
    ::Opaax::from_json(lEngine, lMode);
    CHECK(lMode == EWindowMode::Borderless);

    ERunZone lZone = ERunZone::Start;
    ::Opaax::from_json(lGame, lZone);
    CHECK(lZone == ERunZone::Checkpoint);
}

TEST_CASE("ResolveSupportedBackend: Vulkan is coerced, OpenGL passes through")
{
    CHECK(ResolveSupportedBackend(EBackend::OpenGL) == EBackend::OpenGL);
    CHECK(ResolveSupportedBackend(EBackend::Vulkan) == EBackend::OpenGL);
}
