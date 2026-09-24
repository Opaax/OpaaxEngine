// Suite: OPAAX_ENUM_VALUES — the value list C++20 cannot generate, and the generic json bridge it
// makes possible.
//
// The load-bearing claim is about BYTES: an enum is written as its ToString LABEL, never as an
// ordinal. That is what let two config fields stop being strings without the file moving, and it is
// what keeps inserting an enumerator from silently re-meaning every value already on disk.
#include <doctest.h>

#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Window/Window.h"
#include "RHI/RHIBackend.h"

using namespace Opaax;

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
    CHECK(lJson.get<std::string>() == "Borderless");

    // And it is the SAME text the string-typed config field used to hold, which is why converting it
    // left Engine.config byte-identical.
    CHECK(nlohmann::json(EBackend::OpenGL).get<std::string>() == "OpenGL");
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

    // A typo used to become Windowed with a warning nobody reads. It is now the same event as any
    // other unreadable value in a config: TConfig::Load catches, the defaults stand, ConfigSystem
    // warns naming the file.
    CHECK_THROWS_AS(nlohmann::json("Maximized").get_to(lMode), nlohmann::json::exception);
    CHECK_THROWS_AS(nlohmann::json("").get_to(lMode), nlohmann::json::exception);

    // Wrong TYPE is refused too — a number is not a label, even though the enum has ordinals.
    CHECK_THROWS_AS(nlohmann::json(1).get_to(lMode), nlohmann::json::exception);

    // Nothing was half-applied.
    CHECK(lMode == EWindowMode::Fullscreen);
}

TEST_CASE("ResolveSupportedBackend: Vulkan is coerced, OpenGL passes through")
{
    // The half of the retired BackendFromString that was never about parsing.
    CHECK(ResolveSupportedBackend(EBackend::OpenGL) == EBackend::OpenGL);
    CHECK(ResolveSupportedBackend(EBackend::Vulkan) == EBackend::OpenGL);
}
