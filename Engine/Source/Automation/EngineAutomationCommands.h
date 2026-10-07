#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class ComponentRegistry;
    class Entity;
    class IEngine;
    class World;

    /** What the engine's commands ask of the app hosting them. */
    class IAutomationHost
    {
    public:
        virtual ~IAutomationHost() = default;

        /** Saves the frame being drawn as a PNG at InPath once it is drawn. */
        virtual void RequestScreenshot(const std::string& InPath) = 0;

        /** Closes the app at the end of this frame. */
        virtual void RequestQuit() = 0;

        /** Frames since the app started. */
        virtual Uint64 GetFrameIndex() const = 0;
    };

    // =============================================================================
    // EngineAutomation — the commands every app answers: the app itself (info, quit, screenshot),
    //   input (keys and mouse, fed to the engine as if pressed), and the active world (its
    //   entities, their components, editing a component's values). The editor adds its own, and
    //   replaces the edits with undoable ones.
    // =============================================================================
    namespace EngineAutomation
    {
        void Register(AutomationRunner& InRunner, IEngine& InEngine, IAutomationHost& InHost);

        /**
         * The entity InParams["entity"] names in InWorld: its id (Guid text) or its name (which must
         * be unique). Invalid, with OutError saying why, when there is none.
         */
        Entity FindEntity(World& InWorld, const nlohmann::json& InParams, std::string& OutError);

        /**
         * An entity as JSON: {"id", "name", "parent", "map", "components": [names]}, or with
         * bInWithValues {"components": {name: values}}.
         */
        nlohmann::json DescribeEntity(Entity InEntity, const ComponentRegistry& InTypes, bool bInWithValues);

        /**
         * InEntity's component InParams["type"] with InParams["value"] merged into its values
         * (a JSON merge patch: the fields given change, the others stay). Added when missing.
         * @return False, with OutError, for an unknown type or a value that is not an object
         */
        bool PatchComponent(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InParams,
                            std::string& OutError);
    }
}
