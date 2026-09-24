#include <doctest/doctest.h>

#include <unordered_set>

#include "Core/GUID/Guid.h"

using namespace Opaax;

// =============================================================================
// Guid::Derive — the identity an instance gives one of a template's entities (⑦-C **K2**).
//
//   The property under test is not "it returns something". It is that N instances of one
//   prefab produce N DISTINCT identities, because MapFactory::Instantiate refuses a guid
//   already live in the world (**WM3**) — so a colliding derivation is not a hash quality
//   nitpick, it is an instance that silently fails to appear.
// =============================================================================
TEST_CASE("Guid::Derive is deterministic")
{
    const Guid lInstance = Guid::New();
    const Guid lTemplate = Guid::New();

    CHECK(Guid::Derive(lInstance, lTemplate) == Guid::Derive(lInstance, lTemplate));
}

TEST_CASE("Guid::Derive never yields the invalid sentinel")
{
    // The all-zero pair is the one input that could plausibly fold to zero, so it is the case
    // worth naming rather than a random one.
    CHECK(Guid::Derive(Guid{}, Guid{}).IsValid());
    CHECK(Guid::Derive(Guid::New(), Guid{}).IsValid());
    CHECK(Guid::Derive(Guid{}, Guid::New()).IsValid());
}

TEST_CASE("Guid::Derive separates the two arguments")
{
    const Guid lA = Guid::New();
    const Guid lB = Guid::New();

    // A symmetric mix would make an instance and a template interchangeable, which is wrong and
    // would not show up in any of the distinctness cases below.
    CHECK(Guid::Derive(lA, lB) != Guid::Derive(lB, lA));
}

TEST_CASE("Guid::Derive: two instances of ONE template get different identities")
{
    const Guid lTemplate = Guid::New();
    const Guid lFirst    = Guid::New();
    const Guid lSecond   = Guid::New();

    // THE case the whole mechanism exists for: instantiating one prefab twice into one world.
    CHECK(Guid::Derive(lFirst, lTemplate) != Guid::Derive(lSecond, lTemplate));
}

TEST_CASE("Guid::Derive: one instance keeps a template's entities apart")
{
    const Guid lInstance  = Guid::New();
    const Guid lTemplateA = Guid::New();
    const Guid lTemplateB = Guid::New();

    // A multi-entity prefab: its entities must stay distinct inside a single instance.
    CHECK(Guid::Derive(lInstance, lTemplateA) != Guid::Derive(lInstance, lTemplateB));
}

TEST_CASE("Guid::Derive: a differing 64-bit HALF is enough to separate")
{
    // Both halves of the result fold both words of both inputs, so inputs sharing one half must
    // still diverge. A derivation that mixed High->High and Low->Low would pass every case above
    // and fail these two.
    const Guid lTemplate = Guid::New();

    const Guid lSameHigh{ 0x0123456789abcdefULL, 1 };
    const Guid lSameHigh2{ 0x0123456789abcdefULL, 2 };
    CHECK(Guid::Derive(lSameHigh, lTemplate) != Guid::Derive(lSameHigh2, lTemplate));

    const Guid lSameLow{ 1, 0x0123456789abcdefULL };
    const Guid lSameLow2{ 2, 0x0123456789abcdefULL };
    CHECK(Guid::Derive(lSameLow, lTemplate) != Guid::Derive(lSameLow2, lTemplate));
}

TEST_CASE("Guid::Derive: 100 instances x 20 entities collide nowhere")
{
    // The scale case. Any single pair comparison can pass by luck; 2000 derivations landing in
    // 2000 distinct slots cannot, and this is the shape a real level actually produces.
    TDynArray<Guid> lTemplates;
    for (Int32 lIndex = 0; lIndex < 20; ++lIndex)
    {
        lTemplates.emplace_back(Guid::New());
    }

    std::unordered_set<Guid> lSeen;
    for (Int32 lInstance = 0; lInstance < 100; ++lInstance)
    {
        const Guid lInstanceId = Guid::New();
        for (const Guid& lTemplate : lTemplates)
        {
            lSeen.insert(Guid::Derive(lInstanceId, lTemplate));
        }
    }

    CHECK(lSeen.size() == 100u * 20u);
}

TEST_CASE("Guid::Derive produces an ORDINARY Guid")
{
    // It is written to a map file like any other identity, so it must survive the text form.
    const Guid  lDerived = Guid::Derive(Guid::New(), Guid::New());
    const OpaaxString lText = lDerived.ToString();

    Guid lParsed;
    REQUIRE(Guid::FromString(lText, lParsed));
    CHECK(lParsed == lDerived);
}
