// Suite: UniqueName — the naming rule for new and copied things in editor documents.
#include <doctest.h>

#include "Core/String/UniqueName.h"

using namespace Opaax;

namespace
{
    /** A taken check over a list of names. */
    auto TakenIn(const TDynArray<OpaaxString>& InNames)
    {
        return [&InNames](const OpaaxString& InName)
        {
            for (const OpaaxString& lName : InNames)
            {
                if (lName == InName) { return true; }
            }
            return false;
        };
    }
}

TEST_CASE("UniqueName: BaseOf drops only a trailing space and number")
{
    CHECK(UniqueName::BaseOf(OpaaxString("Crate 12")) == OpaaxString("Crate"));
    CHECK(UniqueName::BaseOf(OpaaxString("Big Crate 3")) == OpaaxString("Big Crate"));
    CHECK(UniqueName::BaseOf(OpaaxString("Crate")) == OpaaxString("Crate"));
    CHECK(UniqueName::BaseOf(OpaaxString("R2D2")) == OpaaxString("R2D2"));
    CHECK(UniqueName::BaseOf(OpaaxString("12")) == OpaaxString("12"));
    CHECK(UniqueName::BaseOf(OpaaxString(" 1")) == OpaaxString(" 1"));
    CHECK(UniqueName::BaseOf(OpaaxString("")) == OpaaxString(""));
}

TEST_CASE("UniqueName: a free name is kept")
{
    const TDynArray<OpaaxString> lNames{ OpaaxString("Other") };
    CHECK(UniqueName::Make(OpaaxString("Crate"), TakenIn(lNames)) == OpaaxString("Crate"));
}

TEST_CASE("UniqueName: a taken name gets the smallest free number")
{
    TDynArray<OpaaxString> lNames{ OpaaxString("Crate"), OpaaxString("Crate 1"), OpaaxString("Crate 3") };
    CHECK(UniqueName::Make(OpaaxString("Crate"), TakenIn(lNames)) == OpaaxString("Crate 2"));

    // A copy of a numbered name counts from its base, not "Crate 1 1".
    CHECK(UniqueName::Make(OpaaxString("Crate 1"), TakenIn(lNames)) == OpaaxString("Crate 2"));
}

TEST_CASE("UniqueName: names made one after another never repeat")
{
    TDynArray<OpaaxString> lNames{ OpaaxString("Finger") };
    for (int lIndex = 0; lIndex < 5; ++lIndex)
    {
        lNames.emplace_back(UniqueName::Make(OpaaxString("Finger"), TakenIn(lNames)));
    }

    for (Uint64 lLeft = 0; lLeft < lNames.size(); ++lLeft)
    {
        for (Uint64 lRight = lLeft + 1; lRight < lNames.size(); ++lRight)
        {
            CHECK(lNames[lLeft] != lNames[lRight]);
        }
    }
    CHECK(lNames.back() == OpaaxString("Finger 5"));
}
