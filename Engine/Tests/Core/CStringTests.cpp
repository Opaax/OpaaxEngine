// Suite: the raw text and byte helpers (Core/String/CString.h, Core/String/Char.h,
// Core/Memory/Memory.h) and OpaaxString::Format: each gives what the C function it names gives.
#include <doctest.h>

#include "Core/Memory/Memory.h"
#include "Core/String/Char.h"
#include "Core/String/CString.h"
#include "Core/String/OpaaxString.hpp"

using namespace Opaax;

TEST_CASE("CString: Length, Compare, Equals, Find")
{
    CHECK(CString::Length("") == 0u);
    CHECK(CString::Length("hello") == 5u);

    CHECK(CString::Compare("abc", "abc") == 0);
    CHECK(CString::Compare("abc", "abd") < 0);
    CHECK(CString::Compare("b", "abc") > 0);
    CHECK(CString::Compare("ab", "abc") < 0);   // a prefix sorts first

    CHECK(CString::Equals("--project", "--project"));
    CHECK_FALSE(CString::Equals("--project", "--projects"));

    const char* lText = "binding = 3";
    REQUIRE(CString::Find(lText, "= ") != nullptr);
    CHECK(CString::Find(lText, "= ") - lText == 8);
    CHECK(CString::Find(lText, "zz") == nullptr);
}

TEST_CASE("CString: ToInt64, ToUint64, ToDouble read a number and say where it stopped")
{
    const char* lEnd = nullptr;

    CHECK(CString::ToInt64("  -42rest", &lEnd) == -42);
    CHECK(CString::Equals(lEnd, "rest"));
    CHECK(CString::ToInt64("7)") == 7);
    CHECK(CString::ToInt64("ff", nullptr, 16) == 255);

    const char* lNoNumber = "abc";
    CHECK(CString::ToInt64(lNoNumber, &lEnd) == 0);
    CHECK(lEnd == lNoNumber);

    CHECK(CString::ToUint64("18446744073709551615") == 18446744073709551615ull);
    CHECK(CString::ToUint64("30") == 30u);

    CHECK(CString::ToDouble("1.5e2px", &lEnd) == doctest::Approx(150.0));
    CHECK(CString::Equals(lEnd, "px"));
}

TEST_CASE("CString::Format: fields like OPAAX_LOG, cut to fit, always terminated")
{
    char lLabel[32];
    CHECK(CString::Format(lLabel, "{}. {}", 3u, "Scale") == 8u);
    CHECK(CString::Equals(lLabel, "3. Scale"));

    CString::Format(lLabel, "{:<6}|{:08X}", "Key", 0xBEEFu);
    CHECK(CString::Equals(lLabel, "Key   |0000BEEF"));

    // Too long: cut at the buffer, the terminator kept.
    char lSmall[6];
    CHECK(CString::Format(lSmall, "{}", "overflowing") == 5u);
    CHECK(CString::Equals(lSmall, "overf"));

    char lOne[1];
    CHECK(CString::Format(lOne, "{}", 42) == 0u);
    CHECK(lOne[0] == '\0');
}

TEST_CASE("Char: classes and case of the C locale, nothing else")
{
    for (const char lSpace : { ' ', '\t', '\n', '\v', '\f', '\r' })
    {
        CHECK(Char::IsSpace(lSpace));
    }
    CHECK_FALSE(Char::IsSpace('a'));
    CHECK_FALSE(Char::IsSpace('\0'));

    CHECK(Char::IsDigit('0'));
    CHECK(Char::IsDigit('9'));
    CHECK_FALSE(Char::IsDigit('a'));
    CHECK(Char::IsAlpha('q'));
    CHECK(Char::IsAlpha('Q'));
    CHECK_FALSE(Char::IsAlpha('_'));
    CHECK(Char::IsAlnum('7'));
    CHECK_FALSE(Char::IsAlnum('-'));

    CHECK(Char::ToLower('A') == 'a');
    CHECK(Char::ToLower('z') == 'z');
    CHECK(Char::ToUpper('m') == 'M');
    CHECK(Char::ToUpper('5') == '5');

    // A byte of a UTF-8 sequence is never a letter and keeps its case.
    const char lUtf8Lead = static_cast<char>(0xC3);
    CHECK_FALSE(Char::IsAlpha(lUtf8Lead));
    CHECK(Char::ToLower(lUtf8Lead) == lUtf8Lead);

    static_assert(Char::IsSpace(' ') && Char::ToUpper('a') == 'A');
}

TEST_CASE("Memory: Copy, CopyOverlapping, Set, Zero, Compare")
{
    Uint8 lSource[4] = { 1, 2, 3, 4 };
    Uint8 lTarget[4] = {};
    Memory::Copy(lTarget, lSource, sizeof(lSource));
    CHECK(Memory::Compare(lTarget, lSource, sizeof(lSource)) == 0);

    // Overlapping: shift left by one.
    Uint8 lWindow[5] = { 1, 2, 3, 4, 5 };
    Memory::CopyOverlapping(lWindow, lWindow + 1, 4);
    CHECK(lWindow[0] == 2);
    CHECK(lWindow[3] == 5);

    Memory::Set(lTarget, 0xAB, sizeof(lTarget));
    CHECK(lTarget[0] == 0xAB);
    CHECK(lTarget[3] == 0xAB);

    float lFloats[3] = { 1.f, 2.f, 3.f };
    Memory::Zero(lFloats, sizeof(lFloats));
    CHECK(lFloats[0] == 0.f);
    CHECK(lFloats[2] == 0.f);

    const Uint8 lLow[2]  = { 1, 1 };
    const Uint8 lHigh[2] = { 1, 2 };
    CHECK(Memory::Compare(lLow, lHigh, 2) < 0);
    CHECK(Memory::Compare(lHigh, lLow, 2) > 0);
}

TEST_CASE("OpaaxString::Format: fields like OPAAX_LOG, any length")
{
    CHECK(OpaaxString::Format("{} of {}", 3, 10) == "3 of 10");
    CHECK(OpaaxString::Format("{:.2f}", 1.0 / 3.0) == "0.33");
    CHECK(OpaaxString::Format("[{}]", OpaaxString("engine")) == "[engine]");
    CHECK(OpaaxString::Format("{}", OpaaxStringView("view")) == "view");
    CHECK(OpaaxString::Format("no fields").GetLength() == 9u);

    const OpaaxString lLong = OpaaxString::Format("{:>40}", "right");
    CHECK(lLong.GetLength() == 40u);
    CHECK(lLong.IsUsingHeap());
}
