// Suite: Margin (Core/Maths/Margin.h) — a value per edge, all zero by default; its file keys are the
// four edges and a missing key keeps its default. UIMargin is the same type under the UI's name.
#include <doctest.h>

#include "Core/Maths/Margin.h"
#include "Engine/Reflection/PropertyJson.h"
#include "UI/UIMargin.h"

using namespace Opaax;

TEST_CASE("Margin: all zero by default, and IsZero says so")
{
    const Margin lNone;
    CHECK(lNone.Left == 0.f);
    CHECK(lNone.Right == 0.f);
    CHECK(lNone.Bottom == 0.f);
    CHECK(lNone.Top == 0.f);
    CHECK(lNone.IsZero());

    CHECK_FALSE(Margin{ 0.f, 0.f, 0.f, 1.f }.IsZero());
    CHECK_FALSE(Margin{ -1.f, 0.f, 0.f, 0.f }.IsZero());
}

TEST_CASE("Margin: written from its properties, under the four edge keys, and read back")
{
    const Margin         lBorder{ 112.f, 110.f, 85.f, 9.f };
    const nlohmann::json lJson = PropertiesToJson(lBorder);

    CHECK(lJson.size() == 4u);
    CHECK(lJson.at("Left").get<float>() == doctest::Approx(112.f));
    CHECK(lJson.at("Right").get<float>() == doctest::Approx(110.f));
    CHECK(lJson.at("Bottom").get<float>() == doctest::Approx(85.f));
    CHECK(lJson.at("Top").get<float>() == doctest::Approx(9.f));

    Margin lBack;
    REQUIRE(PropertiesFromJson(lJson, lBack));
    CHECK(lBack.Left == doctest::Approx(112.f));
    CHECK(lBack.Right == doctest::Approx(110.f));
    CHECK(lBack.Bottom == doctest::Approx(85.f));
    CHECK(lBack.Top == doctest::Approx(9.f));
}

TEST_CASE("Margin: a missing key keeps its default, through properties and through nlohmann")
{
    const nlohmann::json lPartial = { { "Left", 4.f } };

    Margin lRead{ 1.f, 2.f, 3.f, 5.f };
    PropertiesFromJson(lPartial, lRead);
    CHECK(lRead.Left == doctest::Approx(4.f));
    CHECK(lRead.Right == doctest::Approx(2.f));
    CHECK(lRead.Bottom == doctest::Approx(3.f));
    CHECK(lRead.Top == doctest::Approx(5.f));

    const Margin lFromJson = lPartial.get<Margin>();
    CHECK(lFromJson.Left == doctest::Approx(4.f));
    CHECK(lFromJson.Right == 0.f);
    CHECK(lFromJson.Top == 0.f);
}

TEST_CASE("Margin: UIMargin is the same type, so UI code and files are unchanged")
{
    CHECK(TIsSame<UIMargin, Margin>);

    const UIMargin lPadding{ 8.f, 8.f, 4.f, 4.f };
    const Margin&  lAsCore = lPadding;
    CHECK(lAsCore.Bottom == doctest::Approx(4.f));
}
