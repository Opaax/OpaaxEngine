// Suite: the core string types where they meet the standard library — what vendor code hands
// over (std::string, std::string_view) and the stream output doctest prints with. The only test
// file, with Utf8PathTests, that writes std on purpose.
#include <doctest.h>

#include "Core/Hash/OpaaxHash.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"

#include <sstream>
#include <string>
#include <string_view>

using namespace Opaax;

TEST_CASE("OpaaxString: the string_view ctor copies exactly the view")
{
    const std::string_view lView("hello brave new world");

    CHECK(OpaaxString(lView) == "hello brave new world");
    CHECK(OpaaxString(lView.substr(6, 5)) == "brave");   // substr view is NOT null-terminated
    CHECK(OpaaxString(std::string_view{}).IsEmpty());
}

TEST_CASE("OpaaxStringView: a std::string or std::string_view converts implicitly")
{
    const auto lLengthOf = [](OpaaxStringView InView) { return InView.GetLength(); };
    CHECK(lLengthOf(std::string_view("sv", 2)) == 2u);
    CHECK(lLengthOf(std::string("std")) == 3u);
}

TEST_CASE("OpaaxStringView: converts to std::string_view for the vendor boundary")
{
    const OpaaxStringView lSlice = OpaaxStringView("hello world").SubString(6, 5);

    const std::string_view lStd = lSlice;
    CHECK(lStd.size() == 5u);
    CHECK(lStd == "world");

    // A std::string_view reaches a view-taking function through the conversion the other way.
    CHECK(OpaaxHash::Hash64(lSlice) == OpaaxHash::Hash64(std::string_view("world")));

    const std::string_view lEmpty = OpaaxStringView();
    CHECK(lEmpty.empty());
}

TEST_CASE("OpaaxString and OpaaxStringView: stream output writes the text, a slice only its own bytes")
{
    std::ostringstream lStream;
    lStream << OpaaxStringView("abcdef").SubString(0, 3) << '|' << OpaaxString("xyz");
    CHECK(lStream.str() == "abc|xyz");
}
