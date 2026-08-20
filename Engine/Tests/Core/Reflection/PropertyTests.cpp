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
                         OPAAX_PROP(Speed),
                         OPAAX_PROP(Count),
                         OPAAX_PROP(Colour).SetHint(EPropertyHint::Color))
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

TEST_CASE("OPAAX_PROPERTIES: SetHint marks one property and leaves its siblings alone")
{
    constexpr auto lProperties = Probe::GetProperties();

    CHECK(std::get<0>(lProperties).Hint == EPropertyHint::None);
    CHECK(std::get<1>(lProperties).Hint == EPropertyHint::None);
    CHECK(std::get<2>(lProperties).Hint == EPropertyHint::Color);
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
