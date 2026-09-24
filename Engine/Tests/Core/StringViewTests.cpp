// Suite: OpaaxStringView — the one property that matters is that NOTHING reads past the view's
// length. Most cases below therefore run over a slice of a longer buffer, or over bytes with no
// terminator at all: a stray strlen/strcmp/strstr in the implementation passes a naive test and
// fails these ([[L21]] — the instrument must be able to fail).
#include <doctest.h>

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Core/Hash/OpaaxHash.h"   // std::hash + OpaaxHash::Hash for both types

#include <spdlog/fmt/fmt.h>

#include <sstream>
#include <string>
#include <string_view>

using namespace Opaax;

// The constexpr claim, proven where it cannot silently stop being true.
static_assert(OpaaxStringView().IsEmpty());
static_assert(OpaaxStringView("abc").GetLength() == 3);
static_assert(OpaaxStringView("hello world").SubString(0, 5) == "hello");
static_assert(OpaaxStringView("hello world").SubString(0, 5).Find("world") == -1);
static_assert(OpaaxStringView("Maps/Decor.opaaxmap").FindLastOf("/\\") == 4);

TEST_CASE("OpaaxStringView: default view is empty and owns nothing")
{
    constexpr OpaaxStringView lView;

    CHECK(lView.IsEmpty());
    CHECK(lView.GetLength() == 0u);
    CHECK(lView.Data() == nullptr);
    CHECK(lView == "");
    CHECK(lView.ToString().IsEmpty());
}

TEST_CASE("OpaaxStringView: nullptr is an empty view, not a crash")
{
    const char* lNull = nullptr;

    CHECK(OpaaxStringView(lNull).IsEmpty());
    CHECK(OpaaxStringView(lNull, 12).IsEmpty());   // a length with no bytes is still no bytes
    CHECK(OpaaxStringView(lNull) == OpaaxStringView());
}

TEST_CASE("OpaaxStringView: a SLICE never sees the bytes after it")
{
    // The discriminating case. Everything here is true of "hello" and false of "hello world",
    // so an implementation that walks to the terminator fails on the Find and the EndsWith.
    const OpaaxStringView lSlice = OpaaxStringView("hello world").SubString(0, 5);

    CHECK(lSlice.GetLength() == 5u);
    CHECK(lSlice == "hello");
    CHECK(lSlice != "hello world");
    CHECK(lSlice.EndsWith("o"));
    CHECK_FALSE(lSlice.EndsWith("d"));
    CHECK(lSlice.Find("world") == -1);
    CHECK(lSlice.Find('w') == -1);
    CHECK_FALSE(lSlice.Contains("o w"));
    CHECK(lSlice.ToString() == "hello");
    CHECK(lSlice.ToString().GetLength() == 5u);
}

TEST_CASE("OpaaxStringView: bytes with NO terminator anywhere")
{
    const char lRaw[3] = {'a', 'b', 'c'};   // deliberately not a string literal — no '\0' exists
    const OpaaxStringView lView(lRaw, 3);

    CHECK(lView.GetLength() == 3u);
    CHECK(lView == "abc");
    CHECK(lView.StartsWith("ab"));
    CHECK(lView.EndsWith("bc"));
    CHECK(lView.Find('c') == 2);
    CHECK(lView.ToString() == "abc");
    CHECK(fmt::format("{}", lView) == "abc");

    std::ostringstream lStream;
    lStream << lView;
    CHECK(lStream.str() == "abc");
}

TEST_CASE("OpaaxStringView: SubString clamps against the REMAINDER and never wraps")
{
    const OpaaxStringView lView("abcdef");

    CHECK(lView.SubString(2) == "cdef");
    CHECK(lView.SubString(2, 2) == "cd");
    CHECK(lView.SubString(2, UINT32_MAX) == "cdef");   // Start + Length would wrap to 1
    CHECK(lView.SubString(2, 0xFFFFFFF0u) == "cdef");
    CHECK(lView.SubString(2, 0).IsEmpty());
    CHECK(lView.SubString(6).IsEmpty());               // exactly at the end
    CHECK(lView.SubString(99).IsEmpty());
    CHECK(lView.SubString(99, 5).Data() == nullptr);
}

TEST_CASE("OpaaxStringView: RemovePrefix / RemoveSuffix clamp instead of underflowing")
{
    OpaaxStringView lView("abcdef");

    lView.RemovePrefix(2);
    CHECK(lView == "cdef");

    lView.RemoveSuffix(2);
    CHECK(lView == "cd");

    lView.RemovePrefix(99);
    CHECK(lView.IsEmpty());

    OpaaxStringView lOther("xy");
    lOther.RemoveSuffix(99);
    CHECK(lOther.IsEmpty());
}

TEST_CASE("OpaaxStringView: Find answers -1 on a miss, like OpaaxString::Find")
{
    const OpaaxStringView lView("abcabc");

    CHECK(lView.Find("abc") == 0);
    CHECK(lView.Find("abc", 1) == 3);
    CHECK(lView.Find("abc", 4) == -1);
    CHECK(lView.Find("zzz") == -1);
    CHECK(lView.Find("abcabcabc") == -1);          // needle longer than the haystack
    CHECK(lView.Find('b') == 1);
    CHECK(lView.Find('b', 2) == 4);
    CHECK(lView.Find('z') == -1);

    // An empty needle answers the start position, matching std::string_view::find.
    CHECK(lView.Find("") == 0);
    CHECK(lView.Find("", 3) == 3);
    CHECK(lView.Find("", 6) == 6);
    CHECK(lView.Find("", 7) == -1);                // past the end is still a miss
}

TEST_CASE("OpaaxStringView: FindLast walks backwards")
{
    const OpaaxStringView lView("a::b::c");

    CHECK(lView.FindLast("::") == 4);
    CHECK(lView.FindLast(':') == 5);
    CHECK(lView.FindLast("zz") == -1);
    CHECK(lView.FindLast('z') == -1);
    CHECK(OpaaxStringView("nocolon").FindLast("::") == -1);

    CHECK(lView.FindLastOf(":ab") == 5);
    CHECK(OpaaxStringView("C:/Maps\\Decor").FindLastOf("/\\") == 7);
    CHECK(OpaaxStringView("nodots").FindLastOf("/\\") == -1);
}

TEST_CASE("OpaaxStringView: StartsWith / EndsWith / Contains on the edges")
{
    const OpaaxStringView lView("abc");

    CHECK(lView.StartsWith(""));
    CHECK(lView.EndsWith(""));
    CHECK(lView.StartsWith("abc"));
    CHECK(lView.EndsWith("abc"));
    CHECK_FALSE(lView.StartsWith("abcd"));         // longer than the view
    CHECK_FALSE(lView.EndsWith("zabc"));
    CHECK(lView.Contains("b"));
    CHECK_FALSE(lView.Contains("bd"));

    CHECK(OpaaxStringView().StartsWith(""));
    CHECK_FALSE(OpaaxStringView().StartsWith("a"));
}

TEST_CASE("OpaaxStringView: indexing is bounds-checked like OpaaxString's")
{
    const OpaaxStringView lView("ab");

    CHECK(lView[0] == 'a');
    CHECK(lView[1] == 'b');
    CHECK(lView[2] == '\0');                       // out of range, not a read past the end
    CHECK(lView[99] == '\0');
    CHECK(lView.IsValidIndex(1));
    CHECK_FALSE(lView.IsValidIndex(2));
}

TEST_CASE("OpaaxStringView: an OpaaxString converts implicitly, SSO and heap alike")
{
    const OpaaxString lShort("short");                         // SSO
    const OpaaxString lLong("a string well past fifteen characters");
    REQUIRE_FALSE(lShort.IsUsingHeap());
    REQUIRE(lLong.IsUsingHeap());

    // The point of the conversion: a view-taking function accepts a string with no call-site noise.
    const auto lLengthOf = [](OpaaxStringView InView) { return InView.GetLength(); };
    CHECK(lLengthOf(lShort) == lShort.GetLength());
    CHECK(lLengthOf(lLong) == lLong.GetLength());
    CHECK(lLengthOf("literal") == 7u);
    CHECK(lLengthOf(std::string_view("sv", 2)) == 2u);
    CHECK(lLengthOf(std::string("std")) == 3u);

    CHECK(OpaaxStringView(lLong).ToString() == lLong);
    CHECK(OpaaxString(OpaaxStringView(lLong)) == lLong);
}

TEST_CASE("OpaaxStringView: a view over an embedded NUL keeps its full length")
{
    // OpaaxString is a byte container, so a view of it is too — the length wins over the byte.
    const char lRaw[5] = {'a', '\0', 'b', 'c', 'd'};
    const OpaaxStringView lView(lRaw, 5);

    CHECK(lView.GetLength() == 5u);
    CHECK(lView != "a");
    CHECK(lView.Find('b') == 2);
    CHECK(lView.ToString().GetLength() == 5u);
}

TEST_CASE("OpaaxStringView: hashes agree with OpaaxString for the same bytes")
{
    const OpaaxString     lStr("a string well past fifteen characters");
    const OpaaxStringView lView(lStr);

    CHECK(OpaaxHash::Hash(lStr) == OpaaxHash::Hash(lView));
    CHECK(std::hash<OpaaxString>{}(lStr) == std::hash<OpaaxStringView>{}(lView));

    // And the slice must NOT hash like the whole — that is what proves the length is honoured.
    CHECK(OpaaxHash::Hash(lView.SubString(0, 8)) != OpaaxHash::Hash(lView));
}

TEST_CASE("OpaaxStringView: fmt formats the view, not the buffer behind it")
{
    const OpaaxStringView lSlice = OpaaxStringView("hello world").SubString(0, 5);

    CHECK(fmt::format("{}", lSlice) == "hello");
    CHECK(fmt::format("{:>7}", lSlice) == "  hello");   // the base format spec still applies
    CHECK(fmt::format("{}", OpaaxStringView()).empty());
}

TEST_CASE("OpaaxStringView: converts to std::string_view for the vendor boundary")
{
    const OpaaxStringView lSlice = OpaaxStringView("hello world").SubString(6, 5);

    const std::string_view lStd = lSlice;
    CHECK(lStd.size() == 5u);
    CHECK(lStd == "world");

    // Hash64 already takes a std::string_view; a view reaches it through the conversion.
    CHECK(OpaaxHash::Hash64(lSlice) == OpaaxHash::Hash64(std::string_view("world")));

    const std::string_view lEmpty = OpaaxStringView();
    CHECK(lEmpty.empty());
}
