#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"

namespace Opaax
{
    class IEngine;
    class InputMappingSubsystem;

    // =============================================================================
    // InputAutomation — the commands on the player's keys, for tests and agents: list them as a
    //   controls menu does, rebind one, give the assets' keys back and wait for a key (input.keys,
    //   input.rebind, input.resetKeys, input.capture). The keys are the running game's. The answers
    //   are pure functions of its input mapping, tested without an app.
    // =============================================================================
    namespace InputAutomation
    {
        /** Registers input.keys, input.rebind, input.resetKeys and input.capture. */
        void Register(ICommandRegistry& InCommands, IEngine& InEngine);

        /**
         * input.keys {context}: {capturing, captured (the key the last capture took, "None" when it was
         * cancelled, null before any), contexts}, each context known as {context, rebindable, added, keys:
         * [{action, default (the asset's key), key (the player's), rebound}]}; only the one named when given.
         */
        AutomationResult Keys(const InputMappingSubsystem& InMapping, const nlohmann::json& InParams);

        /**
         * input.rebind {action, context, default, key, policy}: gives the mapping the key ("None" clears
         * it) as a controls menu does. {action} alone names a mapping only one known context has;
         * {context} and {default}, the key the asset gives it, tell several apart. {policy} "Refuse" (the
         * default), "Swap" or "Replace" settles a conflict within the context. Answers {result, context,
         * action, default, key, others (the mappings a swap or replace changed)}; a refused rebind fails,
         * its message starting with the result ("Conflict: ...").
         */
        AutomationResult Rebind(InputMappingSubsystem& InMapping, const nlohmann::json& InParams);

        /**
         * input.resetKeys: every mapping gets its asset's key again; {context} only that context's;
         * {action} (with {context} and {default} as input.rebind takes them) only that mapping. Answers as
         * input.keys.
         */
        AutomationResult ResetKeys(InputMappingSubsystem& InMapping, const nlohmann::json& InParams);

        /**
         * input.capture: waits for the next key the player presses, as a menu's "press a key" does
         * (input.key presses one; input.keys then says what it took). {cancel: true} ends the one under
         * way. Answers {capturing, captured}.
         */
        AutomationResult Capture(InputMappingSubsystem& InMapping, const nlohmann::json& InParams);
    }
}
