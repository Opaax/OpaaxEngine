// Suite: Algo, the engine's algorithms over a whole range (Core/Algo/Algo.h): sort, search, count
// and remove on engine containers and C arrays, with the results std gives.
#include <doctest.h>

#include "Core/Algo/Algo.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

namespace
{
    struct Entry
    {
        Int32       Key   = 0;
        OpaaxString Label;
    };
}

TEST_CASE("Algo: Sort, StableSort and Reverse")
{
    TDynArray<Int32> lValues{ 5, 1, 4, 2, 3 };
    Algo::Sort(lValues);
    CHECK(lValues == TDynArray<Int32>{ 1, 2, 3, 4, 5 });

    Algo::Sort(lValues, [](const Int32 InA, const Int32 InB) { return InA > InB; });
    CHECK(lValues == TDynArray<Int32>{ 5, 4, 3, 2, 1 });

    // Equal keys keep their order.
    TDynArray<Entry> lEntries{ { 2, "b1" }, { 1, "a1" }, { 2, "b2" }, { 1, "a2" } };
    Algo::StableSort(lEntries, [](const Entry& InA, const Entry& InB) { return InA.Key < InB.Key; });
    CHECK(lEntries[0].Label == "a1");
    CHECK(lEntries[1].Label == "a2");
    CHECK(lEntries[2].Label == "b1");
    CHECK(lEntries[3].Label == "b2");

    Int32 lArray[] = { 1, 2, 3 };
    Algo::Reverse(lArray);
    CHECK(lArray[0] == 3);
    CHECK(lArray[2] == 1);
}

TEST_CASE("Algo: Find, FindIf, Contains, LowerBound, MaxElement")
{
    TDynArray<Int32> lValues{ 10, 20, 30, 20 };

    const auto lFound = Algo::Find(lValues, 20);
    REQUIRE(lFound != lValues.end());
    CHECK(lFound - lValues.begin() == 1);   // the first of two
    CHECK(Algo::Find(lValues, 99) == lValues.end());

    const auto lOver = Algo::FindIf(lValues, [](const Int32 InValue) { return InValue > 15; });
    REQUIRE(lOver != lValues.end());
    CHECK(*lOver == 20);

    // The iterator can erase what it found.
    lValues.erase(Algo::Find(lValues, 30));
    CHECK(lValues == TDynArray<Int32>{ 10, 20, 20 });

    CHECK(Algo::Contains(lValues, 10));
    CHECK_FALSE(Algo::Contains(lValues, 30));

    const TDynArray<Int32> lSorted{ 1, 3, 3, 7 };
    CHECK(Algo::LowerBound(lSorted, 3) - lSorted.begin() == 1);
    CHECK(Algo::LowerBound(lSorted, 4) - lSorted.begin() == 3);
    CHECK(Algo::LowerBound(lSorted, 9) == lSorted.end());

    const TDynArray<Int32> lDescending{ 9, 5, 5, 1 };
    const auto lGreater = [](const Int32 InA, const Int32 InB) { return InA > InB; };
    CHECK(Algo::LowerBound(lDescending, 5, lGreater) - lDescending.begin() == 1);

    const float lSamples[] = { 0.2f, 0.9f, -0.4f, 0.9f };
    CHECK(Algo::MaxElement(lSamples) - lSamples == 1);   // the first of two
    CHECK(*Algo::MinElement(lSamples) == -0.4f);

    const TDynArray<Int32> lEmpty;
    CHECK(Algo::MaxElement(lEmpty) == lEmpty.end());
}

TEST_CASE("Algo: Count, CountIf, AnyOf, AllOf, NoneOf")
{
    const TFixedArray<bool, 5> lHeard{ true, false, true, true, false };
    CHECK(Algo::Count(lHeard, true) == 3u);
    CHECK(Algo::CountIf(lHeard, [](const bool bInValue) { return !bInValue; }) == 2u);

    const TDynArray<OpaaxString> lNames{ "A:Start", "A:Update", "B:Start" };
    CHECK(Algo::Count(lNames, OpaaxString("A:Start")) == 1u);
    CHECK(Algo::AnyOf(lNames, [](const OpaaxString& InName) { return InName == "B:Start"; }));
    CHECK(Algo::AllOf(lNames, [](const OpaaxString& InName) { return InName.GetLength() > 6; }));
    CHECK(Algo::NoneOf(lNames, [](const OpaaxString& InName) { return InName.IsEmpty(); }));

    const TDynArray<Int32> lEmpty;
    CHECK_FALSE(Algo::AnyOf(lEmpty, [](Int32) { return true; }));
    CHECK(Algo::AllOf(lEmpty, [](Int32) { return false; }));
}

TEST_CASE("Algo: RemoveIf and Unique remove, shrink and say how many")
{
    TDynArray<Int32> lValues{ 1, 2, 3, 4, 5, 6 };
    CHECK(Algo::RemoveIf(lValues, [](const Int32 InValue) { return InValue % 2 == 0; }) == 3u);
    CHECK(lValues == TDynArray<Int32>{ 1, 3, 5 });
    CHECK(Algo::RemoveIf(lValues, [](Int32) { return false; }) == 0u);

    // Maps too: the entries the predicate accepts go.
    TUnorderedMap<Int32, OpaaxString> lMap{ { 1, "keep" }, { 2, "drop" }, { 3, "drop" } };
    CHECK(Algo::RemoveIf(lMap, [](const auto& InPair) { return InPair.second == "drop"; }) == 2u);
    CHECK(lMap.size() == 1u);
    CHECK(lMap.count(1) == 1u);

    // Runs of equal elements keep their first one.
    TDynArray<Int32> lRuns{ 1, 1, 2, 2, 2, 1, 3 };
    CHECK(Algo::Unique(lRuns) == 3u);
    CHECK(lRuns == TDynArray<Int32>{ 1, 2, 1, 3 });

    TDynArray<Entry> lEntries{ { 1, "first" }, { 1, "second" }, { 2, "third" } };
    CHECK(Algo::Unique(lEntries, [](const Entry& InA, const Entry& InB) { return InA.Key == InB.Key; }) == 1u);
    REQUIRE(lEntries.size() == 2u);
    CHECK(lEntries[0].Label == "first");
}
