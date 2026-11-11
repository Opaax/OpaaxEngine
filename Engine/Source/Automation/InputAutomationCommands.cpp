#include "Automation/InputAutomationCommands.h"

#include "Application/Services/IEngine.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/String/OpaaxUtf8.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Input/InputKeyNames.h"
#include "Input/InputManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"

namespace Opaax::InputAutomation
{
    namespace
    {
        /** The running game's input mapping, or null with OutError. */
        InputMappingSubsystem* GameMapping(IEngine& InEngine, OpaaxString& OutError)
        {
            GameInstance* const          lGame    = InEngine.GetGameInstances().GetGameInstance();
            InputMappingSubsystem* const lMapping = lGame != nullptr
                                                        ? lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>()
                                                        : nullptr;
            if (lMapping == nullptr)
            {
                OutError = "no game is running: the keys belong to the game (play a level first)";
            }
            return lMapping;
        }

        /** "Jump on Space in Input/Controls.opaaxasset": a mapping as a message names it. */
        OpaaxString DescribeSlot(const InputKeySlot& InSlot)
        {
            return InSlot.Action.ToString() + " on " + ToString(InSlot.DefaultKey) + " in " + InSlot.Context;
        }

        /** A mapping and its key as the answers give it. */
        nlohmann::json MappingToJson(const InputKeyMapping& InMapping, const bool bInWithContext)
        {
            nlohmann::json lJson{
                { "action",  InMapping.Slot.Action.ToString() },
                { "default", ToString(InMapping.Slot.DefaultKey) },
                { "key",     ToString(InMapping.Key) },
                { "rebound", InMapping.IsRebound() } };
            if (bInWithContext)
            {
                lJson["context"] = InMapping.Slot.Context;
            }
            return lJson;
        }

        /** The key a capture took, as the answers give it: null before any ended. */
        nlohmann::json CapturedToJson(const InputMappingSubsystem& InMapping)
        {
            const TOptional<EKeyCode> lCaptured = InMapping.GetLastCapturedKey();
            return lCaptured.has_value() ? nlohmann::json(ToString(*lCaptured)) : nlohmann::json();
        }

        /** The layouts InContext names: every known one when empty. False with OutError for one not known. */
        bool LayoutsOf(const InputMappingSubsystem& InMapping, const OpaaxString& InContext,
                       TDynArray<InputContextLayout>& OutLayouts, OpaaxString& OutError)
        {
            if (InContext.IsEmpty())
            {
                OutLayouts = InMapping.GetContextLayouts();
                return true;
            }

            if (const InputContextLayout* lLayout = InMapping.FindContextLayout(InContext))
            {
                OutLayouts.push_back(*lLayout);
                return true;
            }

            OutError = "no context asset '" + InContext + "' is known: the game adds or registers it "
                       "(AddContextAsset, RegisterContextAsset); input.keys lists those known";
            return false;
        }

        /**
         * The mapping InParams names: {action}, with {context} and {default} (the asset's key) when
         * several have it. False with OutError saying what to give.
         */
        bool FindSlot(const InputMappingSubsystem& InMapping, const nlohmann::json& InParams, InputKeySlot& OutSlot,
                      OpaaxString& OutError)
        {
            const OpaaxString lAction  = InParams.value("action", OpaaxString());
            const OpaaxString lContext = InParams.value("context", OpaaxString());
            const OpaaxString lDefault = InParams.value("default", OpaaxString());
            if (lAction.IsEmpty())
            {
                OutError = "give the {action} (input.keys lists the mappings)";
                return false;
            }

            EKeyCode lDefaultKey = EKeyCode::None;
            if (!lDefault.IsEmpty() && !KeyFromName(lDefault, lDefaultKey))
            {
                OutError = "'" + lDefault + "' is no key name: {default} is the key the asset gives the mapping";
                return false;
            }

            TDynArray<InputContextLayout> lLayouts;
            if (!LayoutsOf(InMapping, lContext, lLayouts, OutError))
            {
                return false;
            }

            const OpaaxStringID     lActionId(lAction);
            TDynArray<InputKeySlot> lFound;
            for (const InputContextLayout& lLayout : lLayouts)
            {
                for (const InputKeySlot& lSlot : lLayout.Slots)
                {
                    if (lSlot.Action == lActionId && (lDefault.IsEmpty() || lSlot.DefaultKey == lDefaultKey))
                    {
                        lFound.push_back(lSlot);
                    }
                }
            }

            if (lFound.size() == 1)
            {
                OutSlot = lFound[0];
                return true;
            }

            if (lFound.empty())
            {
                OutError = "no known context maps '" + lAction + "'" + (lDefault.IsEmpty() ? OpaaxString() : " on " + lDefault)
                           + (lContext.IsEmpty() ? OpaaxString() : " in " + lContext) + ": input.keys lists the mappings";
                return false;
            }

            OpaaxString lNames;
            for (const InputKeySlot& lSlot : lFound)
            {
                lNames += (lNames.IsEmpty() ? "" : ", ") + DescribeSlot(lSlot);
            }
            OutError = "'" + lAction + "' has several mappings (" + lNames + "): give {context} and {default}, the "
                       "key the asset gives the one to change";
            return false;
        }

        /** The policy InParams names ("Refuse" without one), or false. */
        bool PolicyOf(const nlohmann::json& InParams, EKeyConflictPolicy& OutPolicy)
        {
            const OpaaxString lName = InParams.value("policy", OpaaxString(ToString(EKeyConflictPolicy::Refuse)));
            for (const EKeyConflictPolicy lPolicy : TEnumValues<EKeyConflictPolicy>::Values)
            {
                if (lName == ToString(lPolicy))
                {
                    OutPolicy = lPolicy;
                    return true;
                }
            }
            return false;
        }

        /** "Use on E in Input/Controls.opaaxasset, ...": the mappings a refusal names. */
        OpaaxString DescribeOthers(const TDynArray<InputKeyMapping>& InOthers)
        {
            OpaaxString lNames;
            for (const InputKeyMapping& lOther : InOthers)
            {
                lNames += (lNames.IsEmpty() ? "" : ", ") + DescribeSlot(lOther.Slot);
            }
            return lNames;
        }

        /** Why a rebind of InSlot to InKey was refused, starting with the result, and what to do instead. */
        OpaaxString RefusalText(const InputKeySlot& InSlot, const EKeyCode InKey, const InputRebindResult& InResult)
        {
            const OpaaxString lResult = OpaaxString(ToString(InResult.Result)) + ": ";
            const OpaaxString lKey    = ToString(InKey);

            switch (InResult.Result)
            {
            case ERebindResult::Locked:
                return lResult + InSlot.Context + " is locked (its bRebindable is off): its keys are not the player's";
            case ERebindResult::InvalidKey:
                return lResult + lKey + " is no keyboard key or mouse button";
            case ERebindResult::Conflict:
                return lResult + lKey + " is taken by " + DescribeOthers(InResult.Others)
                       + ": {\"policy\": \"Swap\"} gives it the key " + InSlot.Action.ToString()
                       + " had, \"Replace\" clears it";
            case ERebindResult::Reserved:
                return lResult + lKey + " is taken by " + DescribeOthers(InResult.Others)
                       + ", of a locked context: no policy takes it";
            case ERebindResult::RefusedByGame:
                return lResult + "the game refused " + lKey + " for " + DescribeSlot(InSlot) + ": " + InResult.Why;
            case ERebindResult::NoSuchMapping:
            case ERebindResult::Rebound:
                break;
            }
            return lResult + "no known context has " + DescribeSlot(InSlot) + ": input.keys lists the mappings";
        }
    }

    void Register(ICommandRegistry& InCommands, IEngine& InEngine)
    {
        const auto lWithMapping = [&InEngine](auto InCommand)
        {
            return [&InEngine, InCommand](const nlohmann::json& InParams)
            {
                OpaaxString                  lError;
                InputMappingSubsystem* const lMapping = GameMapping(InEngine, lError);
                return lMapping != nullptr ? InCommand(*lMapping, InParams) : AutomationResult::Fail(lError);
            };
        };

        InCommands.Add(CommandSpec("input.text",
            "Types {text} in one frame, as the OS would for the keys a player presses, for the game's text "
            "fields (the console). Enter, Backspace and the arrows are keys: input.key. Answers {typed}.")
                .Param("text", ECommandParamType::String, "the characters to type"),
            [&InEngine](const nlohmann::json& InParams) { return TypeText(InEngine.GetInput(), InParams); });

        InCommands.Add("input.keys",
            "The running game's keys, as a controls menu lists them: {capturing, captured (the key the last "
            "capture took, \"None\" when cancelled), contexts: [{context, rebindable, added, keys: [{action, "
            "default (the asset's key), key (the player's), rebound}]}]}. Filter: {context}.",
            lWithMapping([](InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
            {
                return Keys(InMapping, InParams);
            }));

        InCommands.Add("input.rebind",
            "Gives a mapping the {key} (\"None\" clears it) as a controls menu does, kept in the player's "
            "settings (not saved): the mapping is {action}, with {context} and {default} (the asset's key) "
            "when several have it; {policy} \"Refuse\" (default), \"Swap\" or \"Replace\" settles a conflict. "
            "Answers {result, context, action, default, key, others}.",
            lWithMapping([](InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
            {
                return Rebind(InMapping, InParams);
            }));

        InCommands.Add("input.resetKeys",
            "Gives every mapping its asset's key again; {context} only that context's, {action} (with "
            "{context} and {default}) only that mapping. Answers as input.keys.",
            lWithMapping([](InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
            {
                return ResetKeys(InMapping, InParams);
            }));

        InCommands.Add("input.capture",
            "Waits for the next key the player presses, as a menu's \"press a key\" does: input.key presses "
            "one, input.keys says what it took. {cancel: true} ends the one under way. Answers {capturing, "
            "captured}.",
            lWithMapping([](InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
            {
                return Capture(InMapping, InParams);
            }));
    }

    AutomationResult Keys(const InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
    {
        OpaaxString                   lError;
        TDynArray<InputContextLayout> lLayouts;
        if (!LayoutsOf(InMapping, InParams.value("context", OpaaxString()), lLayouts, lError))
        {
            return AutomationResult::Fail(lError);
        }

        nlohmann::json lContexts = nlohmann::json::array();
        for (const InputContextLayout& lLayout : lLayouts)
        {
            nlohmann::json lKeys = nlohmann::json::array();
            for (const InputKeyMapping& lMapping : InMapping.GetKeyMappings(lLayout.Context))
            {
                lKeys.push_back(MappingToJson(lMapping, false));
            }

            lContexts.push_back(nlohmann::json{
                { "context",    lLayout.Context },
                { "rebindable", lLayout.bRebindable },
                { "added",      InMapping.IsContextAssetAdded(lLayout.Context) },
                { "keys",       Move(lKeys) } });
        }

        return AutomationResult::Ok(nlohmann::json{
            { "capturing", InMapping.IsCapturingKey() },
            { "captured",  CapturedToJson(InMapping) },
            { "contexts",  Move(lContexts) } });
    }

    AutomationResult Rebind(InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
    {
        OpaaxString  lError;
        InputKeySlot lSlot;
        if (!FindSlot(InMapping, InParams, lSlot, lError))
        {
            return AutomationResult::Fail(lError);
        }

        EKeyCode          lKey  = EKeyCode::None;
        const OpaaxString lName = InParams.value("key", OpaaxString());
        if (!KeyFromName(lName, lKey))
        {
            return AutomationResult::Fail(lName.IsEmpty()
                ? OpaaxString("give the {key}, as input.key names it (\"G\", \"Mouse_Right\"); \"None\" clears it")
                : "'" + lName + "' is no key name: give one as input.key names it (\"G\", \"Mouse_Right\"); \"None\" clears it");
        }

        EKeyConflictPolicy lPolicy = EKeyConflictPolicy::Refuse;
        if (!PolicyOf(InParams, lPolicy))
        {
            return AutomationResult::Fail("\"policy\" is Refuse (the default), Swap or Replace");
        }

        const InputRebindResult lResult = InMapping.Rebind(lSlot, lKey, lPolicy);
        if (!lResult.IsOk())
        {
            return AutomationResult::Fail(RefusalText(lSlot, lKey, lResult));
        }

        nlohmann::json lOthers = nlohmann::json::array();
        for (const InputKeyMapping& lOther : lResult.Others)
        {
            lOthers.push_back(MappingToJson(lOther, true));
        }

        return AutomationResult::Ok(nlohmann::json{
            { "result",  ToString(lResult.Result) },
            { "context", lSlot.Context },
            { "action",  lSlot.Action.ToString() },
            { "default", ToString(lSlot.DefaultKey) },
            { "key",     ToString(InMapping.GetKey(lSlot)) },
            { "others",  Move(lOthers) } });
    }

    AutomationResult ResetKeys(InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
    {
        const OpaaxString lContext = InParams.value("context", OpaaxString());
        OpaaxString       lError;
        if (InParams.contains("action"))
        {
            InputKeySlot lSlot;
            if (!FindSlot(InMapping, InParams, lSlot, lError))
            {
                return AutomationResult::Fail(lError);
            }
            InMapping.ResetKey(lSlot);
        }
        else if (!lContext.IsEmpty())
        {
            TDynArray<InputContextLayout> lLayouts;
            if (!LayoutsOf(InMapping, lContext, lLayouts, lError))
            {
                return AutomationResult::Fail(lError);
            }
            InMapping.ResetKeys(lContext);
        }
        else
        {
            InMapping.ResetAllKeys();
        }

        return Keys(InMapping, lContext.IsEmpty() ? nlohmann::json::object() : nlohmann::json{ { "context", lContext } });
    }

    AutomationResult TypeText(InputManager& InInput, const nlohmann::json& InParams)
    {
        const auto lText = InParams.find("text");
        if (lText == InParams.end() || !lText->is_string() || lText->get<OpaaxString>().IsEmpty())
        {
            return AutomationResult::Fail("give the {text} to type; Enter, Backspace and the arrows are keys: "
                                          "input.key");
        }

        const OpaaxString lUtf8   = lText->get<OpaaxString>();
        const char*       lCursor = lUtf8.CStr();
        const Uint64      lBefore = InInput.GetTypedText().size();
        while (const Uint32 lCodepoint = Utf8::Decode(lCursor))
        {
            InInput.OnCharTyped(lCodepoint);
        }

        // The text lasts its frame: the next request reads what that frame did with it.
        AutomationResult lResult = AutomationResult::Ok(nlohmann::json{
            { "typed", InInput.GetTypedText().size() - lBefore } });
        lResult.WaitFrames = 1;
        return lResult;
    }

    AutomationResult Capture(InputMappingSubsystem& InMapping, const nlohmann::json& InParams)
    {
        if (InParams.value("cancel", false))
        {
            if (!InMapping.IsCapturingKey())
            {
                return AutomationResult::Fail("no capture is under way: input.capture starts one");
            }
            InMapping.CancelKeyCapture();
        }
        else
        {
            InMapping.BeginKeyCapture();
        }

        return AutomationResult::Ok(nlohmann::json{
            { "capturing", InMapping.IsCapturingKey() },
            { "captured",  CapturedToJson(InMapping) } });
    }
}
