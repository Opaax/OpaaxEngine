// Suite: when the Config panel announces an edit (Editor/Panels/ConfigChangeTracker.h) — block CN S2.
//
// Once per COMMITTED edit: a drag is held until release, a switch of config is silent, and a drag
// that ends where it started announces nothing.
#include <doctest.h>

#include "Editor/Panels/ConfigChangeTracker.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    constexpr ConfigTypeID ENGINE_ID   = 1;
    constexpr ConfigTypeID RENDERER_ID = 2;

    constexpr bool EDITING = true;
    constexpr bool IDLE    = false;
}

TEST_CASE("ConfigChangeTracker: the same text frame after frame never fires")
{
    ConfigChangeTracker lTracker;

    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("a"), IDLE));   // first frame = baseline
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("a"), IDLE));
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("a"), IDLE));
}

TEST_CASE("ConfigChangeTracker: a one-frame edit (a checkbox) fires once, then is quiet")
{
    ConfigChangeTracker lTracker;
    lTracker.Update(ENGINE_ID, OpaaxString("off"), IDLE);

    CHECK(lTracker.Update(ENGINE_ID, OpaaxString("on"), IDLE));
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("on"), IDLE));
}

TEST_CASE("ConfigChangeTracker: a DRAG is held for its whole gesture and fires once on release")
{
    ConfigChangeTracker lTracker;
    lTracker.Update(ENGINE_ID, OpaaxString("1.0"), IDLE);

    // The value moves every frame while the mouse is down — none of these may notify.
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("1.1"), EDITING));
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("1.2"), EDITING));
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("1.3"), EDITING));

    CHECK(lTracker.Update(ENGINE_ID, OpaaxString("1.3"), IDLE));        // released
    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("1.3"), IDLE));
}

TEST_CASE("ConfigChangeTracker: a drag that ends where it started announces nothing")
{
    ConfigChangeTracker lTracker;
    lTracker.Update(ENGINE_ID, OpaaxString("1.0"), IDLE);

    lTracker.Update(ENGINE_ID, OpaaxString("1.5"), EDITING);

    CHECK_FALSE(lTracker.Update(ENGINE_ID, OpaaxString("1.0"), IDLE));
}

TEST_CASE("ConfigChangeTracker: switching the shown config is silent, even though the text differs")
{
    ConfigChangeTracker lTracker;
    lTracker.Update(ENGINE_ID, OpaaxString("engine"), IDLE);

    CHECK_FALSE(lTracker.Update(RENDERER_ID, OpaaxString("renderer"), IDLE));

    // ...and the new one is tracked from there.
    CHECK(lTracker.Update(RENDERER_ID, OpaaxString("renderer, edited"), IDLE));
}
