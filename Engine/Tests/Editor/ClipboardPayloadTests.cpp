// Suite: ClipboardPayload — the editor's clipboard text, an envelope naming what it holds.
#include <doctest.h>

#include "Editor/Clipboard/ClipboardPayload.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("ClipboardPayload: what Make writes, Parse reads back")
{
    const nlohmann::json lData = { { "entities", nlohmann::json::array({ 1, 2 }) } };
    const OpaaxString    lText = ClipboardPayload::Make(EClipboardKind::Entities, lData);

    nlohmann::json lRead;
    OpaaxString    lError;
    REQUIRE(ClipboardPayload::Parse(lText, EClipboardKind::Entities, lRead, lError));
    CHECK(lRead == lData);

    // Readable: indented, and says what it holds.
    CHECK(lText.Find("\"opaaxClipboard\": \"Entities\"") >= 0);
}

TEST_CASE("ClipboardPayload: another kind is refused, naming both")
{
    const OpaaxString lText = ClipboardPayload::Make(EClipboardKind::UIWidget, nlohmann::json::object());

    nlohmann::json lRead;
    OpaaxString    lError;
    CHECK_FALSE(ClipboardPayload::Parse(lText, EClipboardKind::Entities, lRead, lError));
    CHECK(lError.Find("UIWidget") >= 0);
    CHECK(lError.Find("Entities") >= 0);
}

TEST_CASE("ClipboardPayload: text from elsewhere is refused without throwing")
{
    nlohmann::json lRead;
    OpaaxString    lError;

    for (const char* lText : { "", "hello", "{", "[1, 2]", "{\"opaaxClipboard\": 3, \"data\": 1}",
                               "{\"opaaxClipboard\": \"Entities\"}", "{\"opaaxClipboard\": \"Nope\", \"data\": 1}" })
    {
        CAPTURE(lText);
        CHECK_FALSE(ClipboardPayload::Parse(OpaaxString(lText), EClipboardKind::Entities, lRead, lError));
        CHECK_FALSE(lError.IsEmpty());
    }
}

TEST_CASE("ClipboardPayload: a newer version is refused")
{
    const OpaaxString lText("{\"opaaxClipboard\": \"Entities\", \"version\": 99, \"data\": {}}");

    nlohmann::json lRead;
    OpaaxString    lError;
    CHECK_FALSE(ClipboardPayload::Parse(lText, EClipboardKind::Entities, lRead, lError));
    CHECK(lError.Find("99") >= 0);
}
