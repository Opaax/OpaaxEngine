// Suite: kerning binary-search (Renderer/Text/FontKerning.h).
//
// KerningLookup was extracted out of FontAsset::GetKerning so it can be tested with a
// hand-built sorted table — no TTF, no FontAsset instance, no GPU. Uses the real
// FontAsset::KerningPair type + baked codepoint range so the test tracks the contract.
#include <doctest.h>

#include "Renderer/Text/FontKerning.h"
#include "Renderer/Text/FontAsset.h" // FontAsset::KerningPair + First/LastCodepoint

using namespace Opaax;
using Kern = FontAsset::KerningPair;

namespace
{
    constexpr Uint32 k_Min = FontAsset::FirstCodepoint; // 0x20
    constexpr Uint32 k_Max = FontAsset::LastCodepoint;  // 0x7E

    float Lookup(const TDynArray<Kern>& InTable, Uint32 InA, Uint32 InB)
    {
        return KerningLookup(InTable.data(), static_cast<Uint32>(InTable.size()),
                             InA, InB, k_Min, k_Max);
    }
}

TEST_CASE("KerningLookup: an empty table returns 0")
{
    const TDynArray<Kern> lEmpty;
    CHECK(Lookup(lEmpty, 'A', 'V') == doctest::Approx(0.f));
}

TEST_CASE("KerningLookup: finds present pairs at the front, middle, and back")
{
    // sorted ascending by PackKerningKey: AV (16726) < To (21615) < Wa (22369)
    const TDynArray<Kern> lTable = {
        { static_cast<Uint8>('A'), static_cast<Uint8>('V'), -2.0f },
        { static_cast<Uint8>('T'), static_cast<Uint8>('o'), -1.5f },
        { static_cast<Uint8>('W'), static_cast<Uint8>('a'), -1.0f },
    };

    CHECK(Lookup(lTable, 'A', 'V') == doctest::Approx(-2.0f));
    CHECK(Lookup(lTable, 'T', 'o') == doctest::Approx(-1.5f));
    CHECK(Lookup(lTable, 'W', 'a') == doctest::Approx(-1.0f));
}

TEST_CASE("KerningLookup: an absent pair returns 0")
{
    const TDynArray<Kern> lTable = {
        { static_cast<Uint8>('A'), static_cast<Uint8>('V'), -2.0f },
        { static_cast<Uint8>('W'), static_cast<Uint8>('a'), -1.0f },
    };

    CHECK(Lookup(lTable, 'A', 'x') == doctest::Approx(0.f)); // present first, wrong second
    CHECK(Lookup(lTable, 'Z', 'Z') == doctest::Approx(0.f)); // not in the table at all
}

TEST_CASE("KerningLookup: codepoints outside the baked range return 0")
{
    const TDynArray<Kern> lTable = {
        { static_cast<Uint8>('A'), static_cast<Uint8>('V'), -2.0f },
    };

    CHECK(Lookup(lTable, 0x10u, 'V')  == doctest::Approx(0.f)); // first below 0x20
    CHECK(Lookup(lTable, 'A',  0x7Fu) == doctest::Approx(0.f)); // second above 0x7E
}

TEST_CASE("KerningLookup: PackKerningKey is order-preserving")
{
    static_assert(PackKerningKey('A', 'V') < PackKerningKey('T', 'o'));
    static_assert(PackKerningKey('T', 'o') < PackKerningKey('W', 'a'));
    CHECK(PackKerningKey(0, 0) == 0u);
    CHECK(PackKerningKey(1, 0) == 256u);
}
