// Suite: OpaaxString SSO/heap behaviour, copy/move, CStr stability, counted Append,
// SubString clamping and the numeric factories.
// OpaaxString is fully header-inline, so this suite compiles it directly.
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "Core/Hash/OpaaxHash.h"   // std::hash<OpaaxString> lives here — see the note in that header

#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

using namespace Opaax;

TEST_CASE("OpaaxString: default-constructed is an empty SSO string")
{
    OpaaxString lStr;
    CHECK(lStr.IsEmpty());
    CHECK(lStr.GetLength() == 0u);
    CHECK_FALSE(lStr.IsUsingHeap());
    CHECK(lStr == "");
    CHECK(lStr.CStr()[0] == '\0');
}

TEST_CASE("OpaaxString: short string stays in SSO")
{
    OpaaxString lStr("hello"); // 5 <= 15
    CHECK(lStr.GetLength() == 5u);
    CHECK_FALSE(lStr.IsUsingHeap());
    CHECK(lStr == "hello");
    CHECK(std::strcmp(lStr.CStr(), "hello") == 0);
}

TEST_CASE("OpaaxString: SSO capacity boundary is 15 chars")
{
    OpaaxString l15("123456789012345"); // exactly 15 -> SSO
    CHECK(l15.GetLength() == 15u);
    CHECK_FALSE(l15.IsUsingHeap());

    OpaaxString l16("1234567890123456"); // 16 -> heap
    CHECK(l16.GetLength() == 16u);
    CHECK(l16.IsUsingHeap());
    CHECK(l16 == "1234567890123456");
}

TEST_CASE("OpaaxString: copy is an independent, equal value")
{
    OpaaxString lA("a heap-allocated string well over fifteen chars");
    REQUIRE(lA.IsUsingHeap());

    OpaaxString lB(lA);
    CHECK(lB == lA);
    CHECK(lB.GetLength() == lA.GetLength());
    CHECK(lB.CStr() != lA.CStr()); // distinct buffers, not a shared pointer
}

TEST_CASE("OpaaxString: move transfers ownership and empties the source")
{
    OpaaxString lA("another heap string definitely beyond SSO capacity");
    REQUIRE(lA.IsUsingHeap());
    const Uint32 lLen = lA.GetLength();

    OpaaxString lB(std::move(lA));
    CHECK(lB.GetLength() == lLen);
    CHECK(lB.IsUsingHeap());
    CHECK(lA.IsEmpty());            // moved-from -> valid empty SSO state
    CHECK_FALSE(lA.IsUsingHeap());
}

TEST_CASE("OpaaxString: append transitions SSO -> heap and preserves content")
{
    OpaaxString lStr("short");
    CHECK_FALSE(lStr.IsUsingHeap());

    lStr.Append("-now this is much longer than fifteen characters");
    CHECK(lStr.IsUsingHeap());
    CHECK(lStr == "short-now this is much longer than fifteen characters");
    CHECK(lStr.GetLength() == static_cast<Uint32>(std::strlen(lStr.CStr())));
}

TEST_CASE("OpaaxString: CStr stays valid + null-terminated across copy assignment")
{
    OpaaxString lA("first value that lives on the heap for sure");
    OpaaxString lB("tiny");
    lB = lA;
    CHECK(std::strlen(lB.CStr()) == lB.GetLength());
    CHECK(lB == lA);
}

// =============================================================================
// Counted Append — the primitive the other overloads forward to
// =============================================================================
TEST_CASE("OpaaxString: counted Append copies exactly Count bytes and stops there")
{
    // Not null-terminated at the cut: anything past Count must never be read.
    const char* lSource = "abcdefghij";

    OpaaxString lStr;
    lStr.Append(lSource, 3);
    CHECK(lStr == "abc");
    CHECK(lStr.GetLength() == 3u);
    CHECK(std::strlen(lStr.CStr()) == 3u);   // terminator written at Count

    lStr.Append(lSource + 3, 2);
    CHECK(lStr == "abcde");
}

TEST_CASE("OpaaxString: counted Append crosses the SSO boundary correctly")
{
    const char* lLong = "0123456789abcdefghijklmnopqrstuvwxyz";   // 36 chars

    OpaaxString lStr("short");
    REQUIRE_FALSE(lStr.IsUsingHeap());

    lStr.Append(lLong, 30);
    CHECK(lStr.IsUsingHeap());
    CHECK(lStr.GetLength() == 35u);
    CHECK(std::strlen(lStr.CStr()) == 35u);
}

TEST_CASE("OpaaxString: Append(nullptr) and zero-count are no-ops")
{
    OpaaxString lStr("keep");
    lStr.Append(nullptr, 4);
    lStr.Append("ignored", 0);
    lStr.Append(nullptr);
    CHECK(lStr == "keep");
}

TEST_CASE("OpaaxString: appending a string TO ITSELF does not read freed memory")
{
    // GrowHeap deletes the buffer the source pointer names, so the old code memcpy'd from freed
    // memory. Both boundary crossings matter: SSO -> heap frees nothing but MOVES the bytes, and
    // heap -> heap frees the block outright.

    SUBCASE("SSO source, growth into the heap")
    {
        OpaaxString lStr("0123456789");         // 10 -> SSO; 20 does not fit
        REQUIRE_FALSE(lStr.IsUsingHeap());

        lStr += lStr;

        CHECK(lStr.IsUsingHeap());
        CHECK(lStr == "01234567890123456789");
        CHECK(lStr.GetLength() == 20u);
    }

    SUBCASE("heap source, reallocation frees the source block")
    {
        OpaaxString lStr("a heap string comfortably past the SSO boundary");
        REQUIRE(lStr.IsUsingHeap());
        const Uint32 lLen = lStr.GetLength();

        lStr.Append(lStr);

        CHECK(lStr.GetLength() == lLen * 2u);
        CHECK(std::strlen(lStr.CStr()) == lLen * 2u);
        CHECK(lStr.SubString(0, lLen) == lStr.SubString(lLen, lLen));
    }

    SUBCASE("a SLICE of our own buffer is just as aliased")
    {
        OpaaxString lStr("0123456789abcdef");   // 16 -> heap
        REQUIRE(lStr.IsUsingHeap());

        lStr.Append(lStr.CStr() + 4, 6);        // "456789", pointing into lStr

        CHECK(lStr == "0123456789abcdef456789");
    }
}

// =============================================================================
// Find bounds
// =============================================================================
TEST_CASE("OpaaxString: Find past the end returns -1 instead of reading on")
{
    const OpaaxString lStr("hello");

    CHECK(lStr.Find("llo")    == 2);
    CHECK(lStr.Find("l", 3)   == 3);
    CHECK(lStr.Find("", 5)    == 5);    // StartPos == Length is legal: the empty tail
    CHECK(lStr.Find("x", 6)   == -1);   // past the terminator — strstr would have walked on
    CHECK(lStr.Find("x", 999) == -1);
    CHECK(lStr.Find(nullptr)  == -1);
}

// =============================================================================
// Counted / view construction
// =============================================================================
TEST_CASE("OpaaxString: the counted ctor stops at Count and never needs a terminator")
{
    const char* lSource = "abcdefghij";

    CHECK(OpaaxString(lSource, 3) == "abc");
    CHECK(OpaaxString(lSource, 3).GetLength() == 3u);
    CHECK(std::strlen(OpaaxString(lSource, 3).CStr()) == 3u);
    CHECK(OpaaxString(lSource, 0).IsEmpty());
    CHECK(OpaaxString(nullptr, 4).IsEmpty());

    // Past SSO, so the heap path is covered too.
    const OpaaxString lLong("0123456789abcdefghijklmnop", 26);
    CHECK(lLong.IsUsingHeap());
    CHECK(lLong.GetLength() == 26u);
}

TEST_CASE("OpaaxString: the string_view ctor copies exactly the view")
{
    const std::string_view lView("hello brave new world");

    CHECK(OpaaxString(lView) == "hello brave new world");
    CHECK(OpaaxString(lView.substr(6, 5)) == "brave");   // substr view is NOT null-terminated
    CHECK(OpaaxString(std::string_view{}).IsEmpty());
}

// =============================================================================
// Hashing — TUnorderedMap<OpaaxString, T> with the DEFAULT hasher
// =============================================================================
TEST_CASE("OpaaxString: std::hash makes it a key without naming OpaaxHash at the call site")
{
    TUnorderedMap<OpaaxString, int> lMap;
    lMap[OpaaxString("alpha")] = 1;
    lMap[OpaaxString("a key long enough to live on the heap")] = 2;

    CHECK(lMap.at(OpaaxString("alpha")) == 1);
    CHECK(lMap.at(OpaaxString("a key long enough to live on the heap")) == 2);
    CHECK(lMap.size() == 2u);

    // Equal values hash equally whichever storage they use — the map would lose entries otherwise.
    lMap[OpaaxString("alpha")] = 3;
    CHECK(lMap.size() == 2u);
    CHECK(std::hash<OpaaxString>{}(OpaaxString("alpha")) == std::hash<OpaaxString>{}(OpaaxString("alpha")));
}

// =============================================================================
// SubString
// =============================================================================
TEST_CASE("OpaaxString: SubString takes a middle slice without over-copying")
{
    const OpaaxString lStr("hello brave new world");

    CHECK(lStr.SubString(6, 5)  == "brave");
    CHECK(lStr.SubString(0, 5)  == "hello");
    CHECK(lStr.SubString(16)    == "world");   // default length = to the end
    CHECK(lStr.SubString(6, 5).GetLength() == 5u);
}

TEST_CASE("OpaaxString: SubString clamps a too-long length instead of overflowing")
{
    const OpaaxString lStr("hello brave new world");   // 21 chars

    // Start + InLength overflows Uint32 here. The old clamp compared that wrapped sum and
    // produced a copy far past the end; clamping against the remainder cannot wrap.
    CHECK(lStr.SubString(6, UINT32_MAX - 2) == "brave new world");
    CHECK(lStr.SubString(6, 999)            == "brave new world");
    CHECK(lStr.SubString(21)   .IsEmpty());   // start == length
    CHECK(lStr.SubString(1000) .IsEmpty());   // start past the end
}

// =============================================================================
// Numeric conversion
// =============================================================================
TEST_CASE("OpaaxString: FromInt / FromUInt cover the range ends")
{
    CHECK(OpaaxString::FromInt(0)     == "0");
    CHECK(OpaaxString::FromInt(42)    == "42");
    CHECK(OpaaxString::FromInt(-42)   == "-42");
    CHECK(OpaaxString::FromUInt(0u)   == "0");
    CHECK(OpaaxString::FromUInt(4242) == "4242");

    // The widest values are what size the stack buffer — 20 digits, plus a sign.
    CHECK(OpaaxString::FromInt(INT64_MIN)   == "-9223372036854775808");
    CHECK(OpaaxString::FromInt(INT64_MAX)   == "9223372036854775807");
    CHECK(OpaaxString::FromUInt(UINT64_MAX) == "18446744073709551615");

    // Past SSO (15), so these must be correct on the heap path too.
    CHECK(OpaaxString::FromUInt(UINT64_MAX).GetLength() == 20u);
    CHECK(OpaaxString::FromUInt(UINT64_MAX).IsUsingHeap());
}

TEST_CASE("OpaaxString: FromUInt composes with operator+ the way call sites use it")
{
    const OpaaxString lName = OpaaxString("SpawnedQuad_") + OpaaxString::FromUInt(7);
    CHECK(lName == "SpawnedQuad_7");
}
