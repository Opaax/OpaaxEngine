// Suite: DevConsoleLineEdit — the console's line and its caret, on codepoints: typing, moving and
// deleting, with characters of several bytes kept whole.
#include <doctest.h>

#include "DevConsole/DevConsoleLineEdit.h"

using namespace Opaax;

TEST_CASE("DevConsoleLineEdit: typing inserts at the caret, which moves past the text")
{
    DevConsoleLineEdit lLine;
    CHECK(lLine.IsEmpty());

    lLine.Insert("time.scale");
    lLine.Insert(" 0.25");
    CHECK(lLine.GetText() == "time.scale 0.25");
    CHECK(lLine.GetCaret() == 15);

    lLine.MoveHome();
    lLine.Insert(">");
    CHECK(lLine.GetText() == ">time.scale 0.25");
    CHECK(lLine.GetTextBeforeCaret() == ">");
}

TEST_CASE("DevConsoleLineEdit: Left, Right, Home and End stay inside the line")
{
    DevConsoleLineEdit lLine;
    lLine.Insert("abc");

    lLine.MoveRight();
    CHECK(lLine.GetCaret() == 3);

    lLine.MoveLeft();
    lLine.MoveLeft();
    CHECK(lLine.GetCaret() == 1);
    CHECK(lLine.GetTextBeforeCaret() == "a");

    lLine.MoveHome();
    lLine.MoveLeft();
    CHECK(lLine.GetCaret() == 0);

    lLine.MoveEnd();
    CHECK(lLine.GetCaret() == 3);
}

TEST_CASE("DevConsoleLineEdit: Backspace deletes before the caret, Delete after it")
{
    DevConsoleLineEdit lLine;
    lLine.Insert("abcd");
    lLine.MoveLeft();
    lLine.MoveLeft();   // ab|cd

    CHECK(lLine.Backspace());
    CHECK(lLine.GetText() == "acd");
    CHECK(lLine.GetCaret() == 1);

    CHECK(lLine.Delete());
    CHECK(lLine.GetText() == "ad");
    CHECK(lLine.GetCaret() == 1);

    lLine.MoveHome();
    CHECK_FALSE(lLine.Backspace());
    lLine.MoveEnd();
    CHECK_FALSE(lLine.Delete());
    CHECK(lLine.GetText() == "ad");
}

TEST_CASE("DevConsoleLineEdit: a character of several bytes moves and deletes as one")
{
    DevConsoleLineEdit lLine;
    lLine.Insert("a\xC3\xA9" "b");   // a, e acute (2 bytes), b
    CHECK(lLine.GetLength() == 3);

    lLine.MoveLeft();
    CHECK(lLine.GetTextBeforeCaret() == "a\xC3\xA9");

    CHECK(lLine.Backspace());
    CHECK(lLine.GetText() == "ab");

    lLine.Insert(TDynArray<Uint32>{ 0x65E5u });   // a CJK character, typed as a codepoint
    CHECK(lLine.GetText() == "a\xE6\x97\xA5" "b");
}

TEST_CASE("DevConsoleLineEdit: SetText replaces the line and puts the caret at its end; line breaks are dropped")
{
    DevConsoleLineEdit lLine;
    lLine.Insert("old");

    lLine.SetText("world.pause\ntrue");
    CHECK(lLine.GetText() == "world.pausetrue");
    CHECK(lLine.GetCaret() == lLine.GetLength());

    lLine.Clear();
    CHECK(lLine.IsEmpty());
    CHECK(lLine.GetCaret() == 0);
}
