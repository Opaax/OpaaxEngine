// Suite: the commands on the player's keys (input.keys, input.rebind, input.resetKeys, input.capture) as
// functions of a game's input mapping, over a walking context and a locked menu written as files.
#include <doctest.h>

#include "../Input/InputTestGame.h"
#include "Automation/InputAutomationCommands.h"

using namespace Opaax;
using namespace InputTesting;

namespace
{
    // A boat context beside the walking one: Use on E and on R.
    const OpaaxString BOAT_PATH("Input/Boat.opaaxasset");

    void WriteBoat(const TestTempDir& InDir)
    {
        InputMappingContextAsset lBoat;
        lBoat.Mappings = { Mapping("Input/Use.opaaxasset", EKeyCode::E), Mapping("Input/Use.opaaxasset", EKeyCode::R) };
        REQUIRE(AssetFile::Save(InDir.Sub("Input/Boat.opaaxasset"), lBoat));
    }

    /** The walking context added and the menu registered, as a game with a controls menu has them; the boat not read. */
    struct KeysFixture
    {
        explicit KeysFixture(const char* InName) : Dir(InName)
        {
            WriteControls(Dir);
            WriteBoat(Dir);
            REQUIRE(Actions.AddContextAsset(OPAAX_ID("Controls"), CONTROLS_PATH));
            REQUIRE(Actions.RegisterContextAsset(MENU_PATH));
        }

        AutomationResult Rebind(const nlohmann::json& InParams) { return InputAutomation::Rebind(Actions, InParams); }

        /** The key input.keys gives InAction's first mapping in InContext. */
        OpaaxString KeyOf(const OpaaxString& InContext, const char* InAction)
        {
            const AutomationResult lKeys = InputAutomation::Keys(Actions, { { "context", InContext } });
            REQUIRE(lKeys.bOk);
            for (const nlohmann::json& lKey : lKeys.Value.at("contexts")[0].at("keys"))
            {
                if (lKey.at("action") == InAction)
                {
                    return lKey.at("key").get<OpaaxString>();
                }
            }
            return OpaaxString();
        }

        TestTempDir            Dir;
        InputClasses           Classes;
        ContextFixture         Game{ Dir.Path(), Classes };
        InputMappingSubsystem& Actions = Game.StartMapping();
    };
}

TEST_CASE("InputAutomation: input.keys lists each known context's keys, as a controls menu does")
{
    KeysFixture lFix("inputKeysList");

    const AutomationResult lKeys = InputAutomation::Keys(lFix.Actions, nlohmann::json::object());
    REQUIRE(lKeys.bOk);
    CHECK(lKeys.Value.at("capturing") == false);
    CHECK(lKeys.Value.at("captured").is_null());

    const nlohmann::json& lContexts = lKeys.Value.at("contexts");
    REQUIRE(lContexts.size() == 2);
    CHECK(lContexts[0] == nlohmann::json::parse(R"({
        "context": "Input/Controls.opaaxasset", "rebindable": true, "added": true,
        "keys": [ { "action": "Jump", "default": "Space", "key": "Space", "rebound": false },
                  { "action": "Use",  "default": "E",     "key": "E",     "rebound": false } ] })"));
    CHECK(lContexts[1].at("context") == "Input/Menu.opaaxasset");
    CHECK(lContexts[1].at("rebindable") == false);
    CHECK(lContexts[1].at("added") == false);

    // One context; one never read is named, with where they come from.
    const AutomationResult lMenu = InputAutomation::Keys(lFix.Actions, { { "context", MENU_PATH } });
    REQUIRE(lMenu.bOk);
    CHECK(lMenu.Value.at("contexts").size() == 1);

    const AutomationResult lUnknown = InputAutomation::Keys(lFix.Actions, { { "context", BOAT_PATH } });
    CHECK_FALSE(lUnknown.bOk);
    CHECK(lUnknown.Error.Contains("RegisterContextAsset"));
}

TEST_CASE("InputAutomation: input.rebind gives a mapping a key, and a refusal says what to do instead")
{
    KeysFixture lFix("inputRebind");

    const AutomationResult lRebound = lFix.Rebind({ { "action", "Jump" }, { "key", "G" } });
    REQUIRE(lRebound.bOk);
    CHECK(lRebound.Value == nlohmann::json::parse(R"({ "result": "Rebound", "context": "Input/Controls.opaaxasset",
        "action": "Jump", "default": "Space", "key": "G", "others": [] })"));
    CHECK(lFix.Game.Drives(lFix.Actions, EKeyCode::G, "Jump"));
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "G");

    // Use has E: refused, then swapped.
    const AutomationResult lConflict = lFix.Rebind({ { "action", "Jump" }, { "key", "E" } });
    CHECK_FALSE(lConflict.bOk);
    CHECK(lConflict.Error.StartsWith("Conflict: E is taken by Use on E in Input/Controls.opaaxasset"));
    CHECK(lConflict.Error.Contains("Swap"));
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "G");

    const AutomationResult lSwap = lFix.Rebind({ { "action", "Jump" }, { "key", "E" }, { "policy", "Swap" } });
    REQUIRE(lSwap.bOk);
    CHECK(lSwap.Value.at("others") == nlohmann::json::parse(R"([ { "context": "Input/Controls.opaaxasset",
        "action": "Use", "default": "E", "key": "G", "rebound": true } ])"));
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Use") == "G");

    // The locked menu's Escape, its own Pause, keys that are none.
    CHECK(lFix.Rebind({ { "action", "Jump" }, { "key", "Escape" }, { "policy", "Replace" } }).Error.StartsWith("Reserved: "));
    CHECK(lFix.Rebind({ { "action", "Pause" }, { "key", "P" } }).Error.StartsWith("Locked: "));
    CHECK(lFix.Rebind({ { "action", "Jump" }, { "key", "AnyKey" } }).Error.StartsWith("InvalidKey: "));
    CHECK(lFix.Rebind({ { "action", "Jump" }, { "key", "Spaec" } }).Error.Contains("'Spaec' is no key name"));
    CHECK(lFix.Rebind({ { "action", "Jump" } }).Error.Contains("give the {key}"));
    CHECK(lFix.Rebind({ { "action", "Jump" }, { "key", "G" }, { "policy", "Steal" } }).Error.Contains("Swap or Replace"));
    CHECK(lFix.Rebind({ { "action", "Dash" }, { "key", "G" } }).Error.Contains("no known context maps 'Dash'"));
    CHECK(lFix.Rebind({ { "key", "G" } }).Error.Contains("give the {action}"));
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "E");

    // The game's check has the last say, and its reason is told.
    lFix.Actions.SetKeyCheck([](const InputKeySlot&, const EKeyCode InKey, OpaaxString& OutWhy)
    {
        OutWhy = OpaaxString("the wheel is the zoom");
        return InKey != EKeyCode::Mouse_WheelUp;
    });
    const AutomationResult lRefused = lFix.Rebind({ { "action", "Jump" }, { "key", "Mouse_WheelUp" } });
    CHECK(lRefused.Error.StartsWith("RefusedByGame: "));
    CHECK(lRefused.Error.Contains("the wheel is the zoom"));

    // None clears it.
    REQUIRE(lFix.Rebind({ { "action", "Jump" }, { "key", "None" } }).bOk);
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "None");
    CHECK_FALSE(lFix.Game.Drives(lFix.Actions, EKeyCode::E, "Jump"));
}

TEST_CASE("InputAutomation: an action with several mappings is named by its context and the asset's key")
{
    KeysFixture lFix("inputRebindSeveral");
    REQUIRE(lFix.Actions.RegisterContextAsset(BOAT_PATH));

    // Use is walking's E, the boat's E and R.
    const AutomationResult lAll = lFix.Rebind({ { "action", "Use" }, { "key", "T" } });
    CHECK_FALSE(lAll.bOk);
    CHECK(lAll.Error.Contains("Use on E in Input/Controls.opaaxasset, Use on E in Input/Boat.opaaxasset, "
                              "Use on R in Input/Boat.opaaxasset"));
    CHECK_FALSE(lFix.Rebind({ { "action", "Use" }, { "context", BOAT_PATH }, { "key", "T" } }).bOk);

    REQUIRE(lFix.Rebind({ { "action", "Use" }, { "context", BOAT_PATH }, { "default", "R" }, { "key", "T" } }).bOk);
    CHECK(lFix.Actions.GetKey(InputKeySlot{ BOAT_PATH, OPAAX_ID("Use"), EKeyCode::R }) == EKeyCode::T);

    // Only the boat has Use on R: its key alone is enough.
    REQUIRE(lFix.Rebind({ { "action", "Use" }, { "default", "R" }, { "key", "Y" } }).bOk);
    CHECK(lFix.Actions.GetKey(InputKeySlot{ BOAT_PATH, OPAAX_ID("Use"), EKeyCode::R }) == EKeyCode::Y);
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Use") == "E");
}

TEST_CASE("InputAutomation: input.resetKeys gives one mapping its asset's key back, or a context's, or all")
{
    KeysFixture lFix("inputResetKeys");
    REQUIRE(lFix.Rebind({ { "action", "Jump" }, { "key", "G" } }).bOk);
    REQUIRE(lFix.Rebind({ { "action", "Use" }, { "key", "F" } }).bOk);

    const AutomationResult lOne = InputAutomation::ResetKeys(lFix.Actions, { { "action", "Jump" } });
    REQUIRE(lOne.bOk);
    CHECK(lOne.Value.at("contexts").size() == 2);
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "Space");
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Use") == "F");

    const AutomationResult lContext = InputAutomation::ResetKeys(lFix.Actions, { { "context", CONTROLS_PATH } });
    REQUIRE(lContext.bOk);
    CHECK(lContext.Value.at("contexts").size() == 1);
    CHECK_FALSE(lFix.Actions.HasRebinds());

    REQUIRE(lFix.Rebind({ { "action", "Use" }, { "key", "F" } }).bOk);
    REQUIRE(InputAutomation::ResetKeys(lFix.Actions, nlohmann::json::object()).bOk);
    CHECK_FALSE(lFix.Actions.HasRebinds());
    CHECK(lFix.Game.Drives(lFix.Actions, EKeyCode::E, "Use"));

    CHECK_FALSE(InputAutomation::ResetKeys(lFix.Actions, { { "context", BOAT_PATH } }).bOk);
    CHECK_FALSE(InputAutomation::ResetKeys(lFix.Actions, { { "action", "Dash" } }).bOk);
}

TEST_CASE("InputAutomation: input.capture waits for the next key, and input.keys says what it took")
{
    KeysFixture lFix("inputCapture");

    const AutomationResult lBegun = InputAutomation::Capture(lFix.Actions, nlohmann::json::object());
    REQUIRE(lBegun.bOk);
    CHECK(lBegun.Value == nlohmann::json::parse(R"({ "capturing": true, "captured": null })"));

    lFix.Game.Press(lFix.Actions, EKeyCode::G);
    const AutomationResult lTaken = InputAutomation::Keys(lFix.Actions, nlohmann::json::object());
    CHECK(lTaken.Value.at("capturing") == false);
    CHECK(lTaken.Value.at("captured") == "G");

    // The key taken changes no key: a menu rebinds with it.
    CHECK(lFix.KeyOf(CONTROLS_PATH, "Jump") == "Space");
    lFix.Game.Release(lFix.Actions, EKeyCode::G);

    REQUIRE(InputAutomation::Capture(lFix.Actions, nlohmann::json::object()).bOk);
    const AutomationResult lCancelled = InputAutomation::Capture(lFix.Actions, { { "cancel", true } });
    REQUIRE(lCancelled.bOk);
    CHECK(lCancelled.Value == nlohmann::json::parse(R"({ "capturing": false, "captured": "None" })"));

    const AutomationResult lNone = InputAutomation::Capture(lFix.Actions, { { "cancel", true } });
    CHECK_FALSE(lNone.bOk);
    CHECK(lNone.Error.Contains("input.capture starts one"));
}
