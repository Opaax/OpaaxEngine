// Suite: OpaaxString SSO/heap behaviour, copy/move, CStr stability.
// OpaaxString is fully header-inline, so this suite compiles it directly.
#include <doctest.h>

#include "Core/OpaaxString.hpp"

#include <cstring>
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
