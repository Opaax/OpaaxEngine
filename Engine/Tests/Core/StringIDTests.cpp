// Suite: OpaaxStringID — the interned-string handle.
//
// "One pool per process" is guaranteed at link time (GetPool() has one definition, in the DLL);
// no single-module test can prove it. What these tests catch is a partial regression: the ctor
// (writer) and PoolSize (reader) ending up in different modules' pools (growth would read 0).
#include <doctest.h>

#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Renderer/RenderLayer.h"

#include <cstring>

using namespace Opaax;

TEST_CASE("OpaaxStringID: default-constructed is the invalid 'None' id")
{
    const OpaaxStringID lId;

    CHECK_FALSE(lId.IsValid());
    CHECK(lId.GetId() == 0u);
    CHECK(lId.ToString() == OpaaxString("None"));
}

TEST_CASE("OpaaxStringID: an empty string interns to None, not to a new entry")
{
    const OpaaxStringID lId(OpaaxString(""));

    CHECK_FALSE(lId.IsValid());
    CHECK(lId.GetId() == 0u);
}

TEST_CASE("OpaaxStringID: the same text always yields the same id (that is the whole point)")
{
    const OpaaxStringID lFirst  = OPAAX_ID("OpaaxStringIDTests_Repeat");
    const OpaaxStringID lSecond = OPAAX_ID("OpaaxStringIDTests_Repeat");

    CHECK(lFirst == lSecond);
    CHECK(lFirst.GetId() == lSecond.GetId());
    CHECK(lFirst.IsValid());
}

TEST_CASE("OpaaxStringID: different text yields different ids, and round-trips back to the text")
{
    const OpaaxStringID lA = OPAAX_ID("OpaaxStringIDTests_Alpha");
    const OpaaxStringID lB = OPAAX_ID("OpaaxStringIDTests_Beta");

    CHECK(lA != lB);
    CHECK(lA.ToString() == OpaaxString("OpaaxStringIDTests_Alpha"));
    CHECK(lB.ToString() == OpaaxString("OpaaxStringIDTests_Beta"));
}

TEST_CASE("OpaaxStringID: one pool across the DLL boundary — a NEW string grows it by exactly 1")
{
    // The ctor writes, PoolSize reads: with a second pool in this module the growth would be 0.
    const Uint32 lBefore = OpaaxStringID::PoolSize();

    const OpaaxStringID lFresh = OPAAX_ID("OpaaxStringIDTests_UniqueGrowthProbe");

    const Uint32 lAfter = OpaaxStringID::PoolSize();

    CHECK(lFresh.IsValid());
    CHECK(lAfter == lBefore + 1u);
}

TEST_CASE("OpaaxStringID: one pool across the DLL boundary — a REPEAT does not grow it")
{
    // Intern once so the entry exists regardless of case order, then measure the repeat.
    const OpaaxStringID lFirst = OPAAX_ID("OpaaxStringIDTests_NoGrowthProbe");

    const Uint32        lBefore = OpaaxStringID::PoolSize();
    const OpaaxStringID lAgain  = OPAAX_ID("OpaaxStringIDTests_NoGrowthProbe");
    const Uint32        lAfter  = OpaaxStringID::PoolSize();

    CHECK(lAgain == lFirst);
    CHECK(lAfter == lBefore);
}

// =============================================================================
// CStr — the zero-copy text
// =============================================================================
TEST_CASE("OpaaxStringID: CStr answers the same bytes as ToString, without the copy")
{
    const OpaaxStringID lId = OPAAX_ID("OpaaxStringIDTests_CStr");

    CHECK(std::strcmp(lId.CStr(), "OpaaxStringIDTests_CStr") == 0);
    CHECK(std::strcmp(lId.CStr(), lId.ToString().CStr()) == 0);
    CHECK(std::strcmp(OpaaxStringID().CStr(), "None") == 0);

    // The pointer is INTO the pool, so it stays valid — and stays the same — across calls.
    CHECK(lId.CStr() == lId.CStr());
    CHECK(lId.CStr() == OPAAX_ID("OpaaxStringIDTests_CStr").CStr());
}

// =============================================================================
// Find — lookup that must not intern
// =============================================================================
TEST_CASE("OpaaxStringID: Find resolves an existing name and does NOT add a missing one")
{
    const OpaaxStringID lInterned = OPAAX_ID("OpaaxStringIDTests_FindHit");

    CHECK(OpaaxStringID::Find(OpaaxString("OpaaxStringIDTests_FindHit")) == lInterned);

    // The whole point: a miss costs nothing permanent. The table is never reclaimed, so a Find that
    // quietly interned would let untrusted text (an editor field, a file scan) grow it without bound.
    const Uint32 lBefore = OpaaxStringID::PoolSize();
    const OpaaxStringID lMiss = OpaaxStringID::Find(OpaaxString("OpaaxStringIDTests_FindMiss_NeverInterned"));

    CHECK_FALSE(lMiss.IsValid());
    CHECK(lMiss.GetId() == 0u);
    CHECK(OpaaxStringID::PoolSize() == lBefore);

    CHECK_FALSE(OpaaxStringID::Find(OpaaxString("")).IsValid());
}

// =============================================================================
// Hashing
// =============================================================================
TEST_CASE("OpaaxStringID: std::hash makes the handle itself a map key")
{
    TUnorderedMap<OpaaxStringID, int> lMap;
    lMap[OPAAX_ID("OpaaxStringIDTests_Key_A")] = 1;
    lMap[OPAAX_ID("OpaaxStringIDTests_Key_B")] = 2;

    CHECK(lMap.at(OPAAX_ID("OpaaxStringIDTests_Key_A")) == 1);
    CHECK(lMap.at(OPAAX_ID("OpaaxStringIDTests_Key_B")) == 2);
    CHECK(lMap.size() == 2u);
}

// =============================================================================
// Storage stability — the guard for the pool's dangling-reference bug
// =============================================================================
TEST_CASE("OpaaxStringID: interned text keeps its ADDRESS while the pool grows")
{
    // Entries must never move: a reader gets a pointer into an entry and then drops the lock.
    // Deterministic on purpose (the threaded case below cannot reliably catch it).
    const OpaaxStringID lShort = OPAAX_ID("zAddr");
    const OpaaxStringID lLong  = OPAAX_ID("zAddr_well_past_the_sso_boundary");

    const char* lShortText = lShort.CStr();
    const char* lLongText  = lLong.CStr();

    // Enough fresh names to drive the id->text array through several reallocations.
    for (int i = 0; i < 4096; ++i)
    {
        const OpaaxStringID lGrow(OpaaxString("zAddr_Growth_") + OpaaxString::FromInt(i));
        REQUIRE(lGrow.IsValid());
    }

    // The short name matters: its bytes live inside the entry, so a moved entry moves them.
    // A long name's heap pointer would survive a move and hide the bug.
    CHECK(lShort.CStr() == lShortText);
    CHECK(lLong.CStr()  == lLongText);
    CHECK(std::strcmp(lShortText, "zAddr") == 0);
    CHECK(std::strcmp(lLongText,  "zAddr_well_past_the_sso_boundary") == 0);
}

// =============================================================================
// Thread safety
// =============================================================================
TEST_CASE("OpaaxStringID: concurrent interning and reading stay consistent")
{
    // The interning contract under contention: the same text from two threads gives one id,
    // and readers never see the wrong text while the table grows.
    constexpr int lWriterCount = 4;
    constexpr int lReaderCount = 4;
    constexpr int lPerWriter   = 250;

    const auto lMakeText = [](int InBucket, int InIndex)
    {
        return OpaaxString("OpaaxStringIDTests_MT_") + OpaaxString::FromInt(InBucket)
             + OpaaxString("_") + OpaaxString::FromInt(InIndex);
    };

    // Seeded up front so the readers have something to resolve while the table grows underneath them.
    // Both storage kinds: a long name (heap, a move keeps the pointer) and a short one (inside
    // the entry, where a move breaks it).
    TDynArray<OpaaxString>   lSeedText;
    TDynArray<OpaaxStringID> lSeedIds;
    for (int i = 0; i < 64; ++i)
    {
        lSeedText.push_back(lMakeText(-1, i));                                    // heap
        lSeedText.push_back(OpaaxString("zMT_") + OpaaxString::FromInt(i));       // SSO
    }
    for (const OpaaxString& lText : lSeedText)
    {
        lSeedIds.emplace_back(lText);
    }

    TAtomic<bool> lStop{false};
    TAtomic<int>  lMismatches{0};

    TDynArray<Thread> lThreads;
    lThreads.reserve(lWriterCount + lReaderCount);

    for (int lW = 0; lW < lWriterCount; ++lW)
    {
        lThreads.emplace_back([&, lW]
        {
            // Writers 0 and 1 share a bucket on purpose: the same text from two threads must give one id.
            const int lBucket = (lW < 2) ? 0 : lW;

            for (int i = 0; i < lPerWriter; ++i)
            {
                const OpaaxString   lText = lMakeText(lBucket, i);
                const OpaaxStringID lId(lText);

                if (lId.ToString() != lText) { ++lMismatches; }
            }
        });
    }

    for (int lR = 0; lR < lReaderCount; ++lR)
    {
        lThreads.emplace_back([&]
        {
            while (!lStop.load(std::memory_order_relaxed))
            {
                for (size_t i = 0; i < lSeedIds.size(); ++i)
                {
                    // Both read paths: CStr returns a pointer into the entry; ToString copies from it after the
                    // lock is dropped.
                    if (std::strcmp(lSeedIds[i].CStr(), lSeedText[i].CStr()) != 0) { ++lMismatches; }
                    if (lSeedIds[i].ToString() != lSeedText[i])                    { ++lMismatches; }
                }
            }
        });
    }

    for (int i = 0; i < lWriterCount; ++i) { lThreads[i].join(); }
    lStop.store(true);
    for (size_t i = lWriterCount; i < lThreads.size(); ++i) { lThreads[i].join(); }

    CHECK(lMismatches.load() == 0);

    // Two writers on bucket 0 must have produced ONE set of ids, not two — so re-interning every
    // distinct text now adds nothing. A lost race would show up here as growth.
    const Uint32 lBefore = OpaaxStringID::PoolSize();
    for (const int lBucket : {0, 2, 3})
    {
        for (int i = 0; i < lPerWriter; ++i)
        {
            CHECK(OpaaxStringID(lMakeText(lBucket, i)) == OpaaxStringID::Find(lMakeText(lBucket, i)));
        }
    }
    CHECK(OpaaxStringID::PoolSize() == lBefore);
}

TEST_CASE("OpaaxStringID: ids interned by engine headers agree with ids interned here")
{
    // g_RenderLayerIDs is built from the same intern table; naming a layer from this module must
    // land on the identical handle. A cheap guard that the canonical-name LUT and ad-hoc call sites
    // cannot drift apart.
    CHECK(OPAAX_ID("Background") == g_RenderLayerIDs[static_cast<Uint8>(ERenderLayer::Background)]);
    CHECK(OPAAX_ID("Debug")      == g_RenderLayerIDs[static_cast<Uint8>(ERenderLayer::Debug)]);
    CHECK(RenderLayerFromStringID(OPAAX_ID("Debug")) == ERenderLayer::Debug);
}

// =============================================================================
// The json bridge (Core/String/OpaaxStringIDJson.h). Its one rule: the TEXT crosses, never the id.
// =============================================================================
TEST_CASE("OpaaxStringID json: a round trip COMPARES EQUAL, and writes the text")
{
    const OpaaxStringID lId = OPAAX_ID("Idle_0");

    const nlohmann::json lJson = lId;

    // The text, not the index. A pool index is built in whatever order a process happened to
    // intern things, so a number written today names a different string tomorrow.
    CHECK(lJson.is_string());
    CHECK(lJson.get<std::string>() == "Idle_0");

    // The assertion that actually matters: equality survives, which is the whole point of interning.
    CHECK(lJson.get<OpaaxStringID>() == lId);
}

TEST_CASE("OpaaxStringID json: an invalid id writes EMPTY and reads back invalid")
{
    // Not "None": CStr() answers "None" for an invalid id, which would read back as a real name.
    const nlohmann::json lJson = OpaaxStringID();

    CHECK(lJson.get<std::string>().empty());
    CHECK_FALSE(lJson.get<OpaaxStringID>().IsValid());
}

TEST_CASE("OpaaxStringID json: reading a name does not depend on it being interned first")
{
    // The file is the writer here — nothing in this process has ever said "Attack_Windup_3".
    const nlohmann::json lJson = std::string("Attack_Windup_3");

    const OpaaxStringID lRead = lJson.get<OpaaxStringID>();

    CHECK(lRead.IsValid());
    CHECK(lRead == OPAAX_ID("Attack_Windup_3"));
}
