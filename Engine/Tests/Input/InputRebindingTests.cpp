// Suite: InputRebinds — the player's keys over the contexts' own, pure. A slot is a context asset, an
// action and the key the asset gives it. Conflicts within a context (refused, swapped or cleared), the
// keys of a locked context reserved, the game's check, and the Keys section's JSON.
#include <doctest.h>

#include "Core/Algo/Algo.h"
#include "Core/String/OpaaxStringJson.h"
#include "Input/Mapping/InputRebinding.h"

using namespace Opaax;

namespace
{
    const char* const CONTROLS = "Input/Controls.opaaxasset";
    const char* const VEHICLE  = "Input/Vehicle.opaaxasset";
    const char* const MENU     = "Input/Menu.opaaxasset";

    InputKeySlot Slot(const char* InContext, const char* InAction, const EKeyCode InDefault)
    {
        return InputKeySlot{ OpaaxString(InContext), OpaaxStringID(OpaaxString(InAction)), InDefault };
    }

    InputContextLayout Layout(const char* InContext, const bool bInRebindable, TDynArray<InputKeySlot> InSlots)
    {
        return InputContextLayout{ OpaaxString(InContext), bInRebindable, Move(InSlots) };
    }

    // A game's contexts: walking (WASD, Space, E), a vehicle using E too, and a locked menu on Escape.
    TDynArray<InputContextLayout> Layouts()
    {
        return {
            Layout(CONTROLS, true, { Slot(CONTROLS, "Move", EKeyCode::W), Slot(CONTROLS, "Move", EKeyCode::S),
                                     Slot(CONTROLS, "Move", EKeyCode::A), Slot(CONTROLS, "Move", EKeyCode::D),
                                     Slot(CONTROLS, "Jump", EKeyCode::Space), Slot(CONTROLS, "Use", EKeyCode::E) }),
            Layout(VEHICLE, true, { Slot(VEHICLE, "Exit", EKeyCode::E) }),
            Layout(MENU, false, { Slot(MENU, "Pause", EKeyCode::Escape) }) };
    }
}

// =============================================================================
// Rebinding
// =============================================================================

TEST_CASE("InputRebinds: a slot takes the key; the asset's key again takes the rebind away")
{
    InputRebinds lKeys;
    const InputKeySlot lJump = Slot(CONTROLS, "Jump", EKeyCode::Space);
    CHECK(lKeys.GetKey(lJump) == EKeyCode::Space);

    const InputRebindResult lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::F, EKeyConflictPolicy::Refuse);
    CHECK(lResult.IsOk());
    CHECK(lResult.Others.empty());
    CHECK(lKeys.GetKey(lJump) == EKeyCode::F);
    CHECK(lKeys.Num() == 1);
    CHECK(lKeys.HasRebinds(OpaaxString(CONTROLS)));
    CHECK_FALSE(lKeys.HasRebinds(OpaaxString(VEHICLE)));

    // The key it has already: nothing to do.
    CHECK(lKeys.Rebind(Layouts(), lJump, EKeyCode::F, EKeyConflictPolicy::Refuse).IsOk());
    CHECK(lKeys.Num() == 1);

    CHECK(lKeys.Rebind(Layouts(), lJump, EKeyCode::Space, EKeyConflictPolicy::Refuse).IsOk());
    CHECK(lKeys.IsEmpty());
}

TEST_CASE("InputRebinds: a key another mapping of the context has is refused, swapped or taken from it")
{
    const InputKeySlot lJump = Slot(CONTROLS, "Jump", EKeyCode::Space);
    const InputKeySlot lUse  = Slot(CONTROLS, "Use", EKeyCode::E);

    SUBCASE("Refuse: nothing changes, and the menu learns which mapping has it")
    {
        InputRebinds lKeys;
        const InputRebindResult lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::E, EKeyConflictPolicy::Refuse);
        CHECK(lResult.Result == ERebindResult::Conflict);
        REQUIRE(lResult.Others.size() == 1);
        CHECK(lResult.Others[0].Slot == lUse);
        CHECK(lResult.Others[0].Key == EKeyCode::E);
        CHECK(lKeys.IsEmpty());
    }

    SUBCASE("Swap: the other takes the key the rebound one had")
    {
        InputRebinds lKeys;
        const InputRebindResult lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::E, EKeyConflictPolicy::Swap);
        CHECK(lResult.IsOk());
        REQUIRE(lResult.Others.size() == 1);
        CHECK(lResult.Others[0].Key == EKeyCode::Space);
        CHECK(lKeys.GetKey(lJump) == EKeyCode::E);
        CHECK(lKeys.GetKey(lUse) == EKeyCode::Space);
    }

    SUBCASE("Replace: the other loses the key, and drives nothing")
    {
        InputRebinds lKeys;
        const InputRebindResult lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::E, EKeyConflictPolicy::Replace);
        CHECK(lResult.IsOk());
        REQUIRE(lResult.Others.size() == 1);
        CHECK(lResult.Others[0].Key == EKeyCode::None);
        CHECK(lKeys.GetKey(lJump) == EKeyCode::E);
        CHECK(lKeys.GetKey(lUse) == EKeyCode::None);
    }
}

TEST_CASE("InputRebinds: two keys of one action swap, as a player flips the arrows")
{
    InputRebinds       lKeys;
    const InputKeySlot lUp   = Slot(CONTROLS, "Move", EKeyCode::W);
    const InputKeySlot lDown = Slot(CONTROLS, "Move", EKeyCode::S);

    CHECK(lKeys.Rebind(Layouts(), lUp, EKeyCode::S, EKeyConflictPolicy::Refuse).Result == ERebindResult::Conflict);

    REQUIRE(lKeys.Rebind(Layouts(), lUp, EKeyCode::S, EKeyConflictPolicy::Swap).IsOk());
    CHECK(lKeys.GetKey(lUp) == EKeyCode::S);
    CHECK(lKeys.GetKey(lDown) == EKeyCode::W);

    // Back: both slots have their asset's key, so nothing is kept.
    REQUIRE(lKeys.Rebind(Layouts(), lUp, EKeyCode::W, EKeyConflictPolicy::Swap).IsOk());
    CHECK(lKeys.IsEmpty());
}

TEST_CASE("InputRebinds: another context on the key is no conflict; a locked one's key is never taken")
{
    InputRebinds lKeys;

    // The vehicle's Exit is on E too: those contexts are not active together, or one is above the other.
    CHECK(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Jump", EKeyCode::Space), EKeyCode::Q, EKeyConflictPolicy::Refuse).IsOk());
    CHECK(lKeys.Rebind(Layouts(), Slot(VEHICLE, "Exit", EKeyCode::E), EKeyCode::Q, EKeyConflictPolicy::Refuse).IsOk());

    // The menu's Escape, whatever the policy.
    for (const EKeyConflictPolicy lPolicy : { EKeyConflictPolicy::Refuse, EKeyConflictPolicy::Swap, EKeyConflictPolicy::Replace })
    {
        CAPTURE(ToString(lPolicy));
        const InputRebindResult lResult = lKeys.Rebind(Layouts(), Slot(CONTROLS, "Use", EKeyCode::E), EKeyCode::Escape, lPolicy);
        CHECK(lResult.Result == ERebindResult::Reserved);
        REQUIRE(lResult.Others.size() == 1);
        CHECK(lResult.Others[0].Slot == Slot(MENU, "Pause", EKeyCode::Escape));
    }
    CHECK(lKeys.GetKey(Slot(CONTROLS, "Use", EKeyCode::E)) == EKeyCode::E);

    // And its own keys are not the player's.
    CHECK(lKeys.Rebind(Layouts(), Slot(MENU, "Pause", EKeyCode::Escape), EKeyCode::P, EKeyConflictPolicy::Refuse).Result
          == ERebindResult::Locked);
}

TEST_CASE("InputRebinds: a mapping cleared, a key that is none, a mapping no context has")
{
    InputRebinds lKeys;
    const InputKeySlot lJump = Slot(CONTROLS, "Jump", EKeyCode::Space);

    // Cleared: no conflict to look for.
    CHECK(lKeys.Rebind(Layouts(), lJump, EKeyCode::None, EKeyConflictPolicy::Refuse).IsOk());
    CHECK(lKeys.GetKey(lJump) == EKeyCode::None);

    CHECK(lKeys.Rebind(Layouts(), lJump, EKeyCode::AnyKey, EKeyConflictPolicy::Refuse).Result == ERebindResult::InvalidKey);
    CHECK(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Jump", EKeyCode::J), EKeyCode::K, EKeyConflictPolicy::Refuse).Result
          == ERebindResult::NoSuchMapping);
    CHECK(lKeys.Rebind(Layouts(), Slot("Input/Nope.opaaxasset", "Jump", EKeyCode::Space), EKeyCode::K,
                       EKeyConflictPolicy::Refuse).Result == ERebindResult::NoSuchMapping);
    CHECK(lKeys.GetKey(lJump) == EKeyCode::None);
}

TEST_CASE("InputRebinds: the game's check has the last say, on the key swapped too")
{
    // The game keeps the mouse wheel off Jump, and Space off Use.
    const FInputKeyCheck lCheck = [](const InputKeySlot& InSlot, const EKeyCode InKey, OpaaxString& OutWhy)
    {
        if (InSlot.Action == OPAAX_ID("Jump") && InKey == EKeyCode::Mouse_WheelUp)
        {
            OutWhy = OpaaxString("Jump is held: the wheel cannot hold it");
            return false;
        }
        if (InSlot.Action == OPAAX_ID("Use") && InKey == EKeyCode::Space)
        {
            OutWhy = OpaaxString("Space is the jump key");
            return false;
        }
        return true;
    };

    InputRebinds       lKeys;
    const InputKeySlot lJump = Slot(CONTROLS, "Jump", EKeyCode::Space);

    InputRebindResult lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::Mouse_WheelUp, EKeyConflictPolicy::Refuse, lCheck);
    CHECK(lResult.Result == ERebindResult::RefusedByGame);
    CHECK(lResult.Why == "Jump is held: the wheel cannot hold it");

    // Swapping would give Use the Space key.
    lResult = lKeys.Rebind(Layouts(), lJump, EKeyCode::E, EKeyConflictPolicy::Swap, lCheck);
    CHECK(lResult.Result == ERebindResult::RefusedByGame);
    CHECK(lResult.Why == "Space is the jump key");
    CHECK(lResult.Others.empty());
    CHECK(lKeys.IsEmpty());

    // Clearing Use instead is allowed.
    CHECK(lKeys.Rebind(Layouts(), lJump, EKeyCode::E, EKeyConflictPolicy::Replace, lCheck).IsOk());
}

TEST_CASE("InputRebinds: resets, by slot, by context, all")
{
    InputRebinds lKeys;
    REQUIRE(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Jump", EKeyCode::Space), EKeyCode::F, EKeyConflictPolicy::Refuse).IsOk());
    REQUIRE(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Use", EKeyCode::E), EKeyCode::G, EKeyConflictPolicy::Refuse).IsOk());
    REQUIRE(lKeys.Rebind(Layouts(), Slot(VEHICLE, "Exit", EKeyCode::E), EKeyCode::X, EKeyConflictPolicy::Refuse).IsOk());

    lKeys.Reset(Slot(CONTROLS, "Jump", EKeyCode::Space));
    CHECK(lKeys.GetKey(Slot(CONTROLS, "Jump", EKeyCode::Space)) == EKeyCode::Space);
    CHECK(lKeys.Num() == 2);

    lKeys.ResetContext(OpaaxString(CONTROLS));
    CHECK(lKeys.Num() == 1);
    CHECK(lKeys.GetKey(Slot(VEHICLE, "Exit", EKeyCode::E)) == EKeyCode::X);

    lKeys.ResetAll();
    CHECK(lKeys.IsEmpty());
}

// =============================================================================
// The Keys section
// =============================================================================

TEST_CASE("InputRebinds: the Keys section, by context, action and the asset's key; read back the same")
{
    InputRebinds lKeys;
    CHECK(lKeys.ToJson().is_null());

    REQUIRE(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Move", EKeyCode::W), EKeyCode::Up, EKeyConflictPolicy::Refuse).IsOk());
    REQUIRE(lKeys.Rebind(Layouts(), Slot(CONTROLS, "Move", EKeyCode::S), EKeyCode::None, EKeyConflictPolicy::Refuse).IsOk());
    REQUIRE(lKeys.Rebind(Layouts(), Slot(VEHICLE, "Exit", EKeyCode::E), EKeyCode::Mouse_Right, EKeyConflictPolicy::Refuse).IsOk());

    const nlohmann::json lJson = lKeys.ToJson();
    CHECK(lJson == nlohmann::json::parse(R"({
        "Input/Controls.opaaxasset": { "Move": { "W": "Up", "S": "None" } },
        "Input/Vehicle.opaaxasset":  { "Exit": { "E": "Mouse_Right" } } })"));

    InputRebinds           lRead;
    TDynArray<OpaaxString> lDropped;
    lRead.FromJson(lJson, &lDropped);
    CHECK(lDropped.empty());
    CHECK(lRead.Num() == 3);
    CHECK(lRead.GetKey(Slot(CONTROLS, "Move", EKeyCode::W)) == EKeyCode::Up);
    CHECK(lRead.GetKey(Slot(CONTROLS, "Move", EKeyCode::S)) == EKeyCode::None);
    CHECK(lRead.GetKey(Slot(VEHICLE, "Exit", EKeyCode::E)) == EKeyCode::Mouse_Right);
    CHECK(lRead.ToJson() == lJson);
}

TEST_CASE("InputRebinds: what is not a key in the section is dropped, and said; the rest is read")
{
    InputRebinds           lKeys;
    TDynArray<OpaaxString> lDropped;
    lKeys.FromJson(nlohmann::json::parse(R"({
        "Input/Controls.opaaxasset": { "Jump": { "Space": "Spaec", "Spase": "F" }, "Use": { "E": 3 }, "Move": { "W": "Up" } },
        "Input/Vehicle.opaaxasset": [ "E" ] })"), &lDropped);

    CHECK(lKeys.Num() == 1);
    CHECK(lKeys.GetKey(Slot(CONTROLS, "Move", EKeyCode::W)) == EKeyCode::Up);
    REQUIRE(lDropped.size() == 4);
    CHECK(Algo::Contains(lDropped, OpaaxString("Input/Controls.opaaxasset: Jump on Space: 'Spaec' is no key name")));
    CHECK(Algo::Contains(lDropped, OpaaxString("Input/Vehicle.opaaxasset: not an object of actions")));

    // Not an object at all: nothing read.
    lDropped.clear();
    lKeys.FromJson(nlohmann::json::parse("[1, 2]"), &lDropped);
    CHECK(lKeys.IsEmpty());
    CHECK(lDropped.size() == 1);

    // No section: no rebind, nothing said.
    lDropped.clear();
    lKeys.FromJson(nullptr, &lDropped);
    CHECK(lDropped.empty());
}

TEST_CASE("InputRebinds: once a context is known, the keys it does not allow are dropped")
{
    InputRebinds lKeys;
    lKeys.FromJson(nlohmann::json::parse(R"({
        "Input/Controls.opaaxasset": { "Jump": { "Space": "F" }, "Dash": { "LeftShift": "G" } },
        "Input/Menu.opaaxasset": { "Pause": { "Escape": "P" } },
        "Input/Later.opaaxasset": { "Fly": { "F": "V" } } })"));
    REQUIRE(lKeys.Num() == 4);

    TDynArray<OpaaxString> lDropped;
    for (const InputContextLayout& lLayout : Layouts())
    {
        lKeys.DropUnknown(lLayout, &lDropped);
    }

    // Dash is no longer in the asset; the menu is locked; a context not known yet keeps its keys.
    CHECK(lKeys.Num() == 2);
    CHECK(lKeys.GetKey(Slot(CONTROLS, "Jump", EKeyCode::Space)) == EKeyCode::F);
    CHECK(lKeys.GetKey(Slot("Input/Later.opaaxasset", "Fly", EKeyCode::F)) == EKeyCode::V);
    CHECK(Algo::Contains(lDropped, OpaaxString("Input/Controls.opaaxasset: Dash on LeftShift: the context has no such mapping")));
    CHECK(Algo::Contains(lDropped, OpaaxString("Input/Menu.opaaxasset: Pause on Escape: the context is locked")));
}
