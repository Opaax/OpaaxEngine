#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"

namespace Opaax
{
    class IEngine;
    class ISaveStorage;
    class ObjectClassRegistry;

    // =============================================================================
    // SaveAutomation — the commands on the player's save slots, for tests and agents: list, read,
    //   write and clear a slot without playing (save.list, save.get, save.set, save.clear). While a
    //   game runs, each first waits for its background saves (SaveGameSubsystem::Flush). The answers
    //   are pure functions of the storage, tested without an app.
    // =============================================================================
    namespace SaveAutomation
    {
        /** Registers save.list, save.get, save.set and save.clear. */
        void Register(AutomationRunner& InRunner, IEngine& InEngine);

        /** save.list: {folder, slots}, the slots sorted. */
        AutomationResult List(const ISaveStorage& InStorage);

        /**
         * save.get {slot}: {slot, path, class, classVersion, properties}, read from the slot's file as it
         * is. Fails for a slot without a save, or a file that is not a save this engine reads.
         */
        AutomationResult Get(const ISaveStorage& InStorage, const nlohmann::json& InParams);

        /**
         * save.set {slot, class, properties}: writes a save of class, at its defaults with properties
         * merged, as the game writes it; an unknown class, an unknown property or a value of the wrong
         * type writes nothing. save.set {slot, text}: writes text as given, unchecked (an old or broken
         * file, for a test). Answers {slot, path}.
         */
        AutomationResult Set(ISaveStorage& InStorage, const ObjectClassRegistry& InClasses, const nlohmann::json& InParams);

        /** save.clear {slot} or {all: true}: deletes the saves, answering {deleted: [slots]}. */
        AutomationResult Clear(ISaveStorage& InStorage, const nlohmann::json& InParams);
    }
}
