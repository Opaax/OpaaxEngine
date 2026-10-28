// Suite: RandomStream. The seeded generator sounds and frame pacing draw from: a seed replays.
#include <doctest.h>

#include "Core/Maths/RandomStream.h"

using namespace Opaax;

TEST_CASE("RandomStream: the same seed gives the same numbers; ranges hold")
{
    RandomStream lA(42);
    RandomStream lB(42);
    RandomStream lC(43);

    bool bSame      = true;
    bool bDifferent = false;
    for (int lIndex = 0; lIndex < 100; ++lIndex)
    {
        const Uint32 lValue = lA.NextUint32();
        bSame      = bSame && lValue == lB.NextUint32();
        bDifferent = bDifferent || lValue != lC.NextUint32();
    }
    CHECK(bSame);
    CHECK(bDifferent);

    for (int lIndex = 0; lIndex < 1000; ++lIndex)
    {
        const float lFloat = lA.Range(-0.1f, 0.1f);
        CHECK(lFloat >= -0.1f);
        CHECK(lFloat < 0.1f);
        CHECK(lA.Below(5) < 5);
    }
    CHECK(lA.Below(0) == 0);
}
