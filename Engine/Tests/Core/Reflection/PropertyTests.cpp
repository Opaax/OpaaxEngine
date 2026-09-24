// Suite: OPAAX_PROPERTIES — the per-type field list the editor's default drawer folds over.
// Engine-side on purpose: the list is DATA (no ImGui, no editor), so the part that can be tested
// is the part that matters — the names, the member pointers, and that the whole thing is usable in
// constant evaluation.
#include <doctest.h>

#include <type_traits>

#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

namespace
{
    struct Probe
    {
        float    Speed  = 1.f;
        Int32    Count  = 2;
        Vector4F Colour = {1.f, 1.f, 1.f, 1.f};

        OPAAX_PROPERTIES(Probe,
                         OPAAX_PROP(Speed).SetRange(0.f, 10.f).SetTooltip("Units per second."),
                         OPAAX_PROP(Count),
                         OPAAX_PROP(Colour))
    };

    struct NoProperties
    {
        float Speed = 1.f;
    };

    // The list must be a CONSTANT EXPRESSION, not merely const: the editor reads its size at compile
    // time to log a count, and nothing may end up in a shipped binary that is not referenced.
    static_assert(std::tuple_size_v<decltype(Probe::GetProperties())> == 3);
    static_assert(CReflected<Probe>);
    static_assert(!CReflected<NoProperties>);
}

TEST_CASE("OPAAX_PROPERTIES: names are the field names, in declaration order")
{
    constexpr auto lProperties = Probe::GetProperties();

    CHECK(OpaaxString(std::get<0>(lProperties).Name) == "Speed");
    CHECK(OpaaxString(std::get<1>(lProperties).Name) == "Count");
    CHECK(OpaaxString(std::get<2>(lProperties).Name) == "Colour");
}

TEST_CASE("OPAAX_PROPERTIES: the member pointer reads AND writes the field it names")
{
    Probe lProbe;
    constexpr auto lProperties = Probe::GetProperties();

    // Read through the description.
    CHECK(lProbe.*(std::get<0>(lProperties).Member) == doctest::Approx(1.f));
    CHECK(lProbe.*(std::get<1>(lProperties).Member) == 2);

    // Write through it — this is what a drawer does, and a member pointer aimed at the wrong field
    // would pass every read-only check above.
    lProbe.*(std::get<0>(lProperties).Member) = 42.f;
    lProbe.*(std::get<1>(lProperties).Member) = 7;

    CHECK(lProbe.Speed == doctest::Approx(42.f));
    CHECK(lProbe.Count == 7);
    CHECK(lProbe.Colour.r == doctest::Approx(1.f));   // untouched
}

TEST_CASE("OPAAX_PROPERTIES: SetRange marks one property and leaves its siblings alone")
{
    constexpr auto lProperties = Probe::GetProperties();

    CHECK(std::get<0>(lProperties).Meta.RangeMin == doctest::Approx(0.f));
    CHECK(std::get<0>(lProperties).Meta.RangeMax == doctest::Approx(10.f));

    // Unset is Min == Max, which every ImGui drag reads as "unbounded" — so an ordinary property
    // needs no flag saying it has no range.
    CHECK(std::get<1>(lProperties).Meta.RangeMin == std::get<1>(lProperties).Meta.RangeMax);
    CHECK(std::get<2>(lProperties).Meta.RangeMin == std::get<2>(lProperties).Meta.RangeMax);
}

TEST_CASE("OPAAX_PROPERTIES: SetTooltip carries the text and chains with the other facets")
{
    constexpr auto lProperties = Probe::GetProperties();

    // Chained onto SetRange, so neither facet may drop the other's work — every facet returns a
    // modified COPY, which is what makes the order they are written in irrelevant.
    CHECK(OpaaxString(std::get<0>(lProperties).Meta.Tooltip) == "Units per second.");
    CHECK(std::get<0>(lProperties).Meta.RangeMax == doctest::Approx(10.f));

    // Absent is null, so a property with nothing to explain draws no marker.
    CHECK(std::get<1>(lProperties).Meta.Tooltip == nullptr);
}

TEST_CASE("OPAAX_PROPERTIES: the value type comes from the member pointer, not from the author")
{
    constexpr auto lProperties = Probe::GetProperties();

    using Speed  = std::remove_cvref_t<decltype(std::get<0>(lProperties))>;
    using Count  = std::remove_cvref_t<decltype(std::get<1>(lProperties))>;
    using Colour = std::remove_cvref_t<decltype(std::get<2>(lProperties))>;

    static_assert(std::is_same_v<Speed::ValueType, float>);
    static_assert(std::is_same_v<Count::ValueType, Int32>);
    static_assert(std::is_same_v<Colour::ValueType, Vector4F>);
    static_assert(std::is_same_v<Speed::ClassType, Probe>);

    CHECK(true); // the assertions above are the test
}
