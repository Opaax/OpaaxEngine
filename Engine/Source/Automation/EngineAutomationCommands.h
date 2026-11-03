#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class ComponentRegistry;
    class Entity;
    class IEngine;
    class UICanvas;
    class World;

    /** What the engine's commands ask of the app hosting them. */
    class IAutomationHost
    {
    public:
        virtual ~IAutomationHost() = default;

        /** Saves the frame being drawn as a PNG at InPath once it is drawn. */
        virtual void RequestScreenshot(const OpaaxString& InPath) = 0;

        /** Closes the app at the end of this frame. */
        virtual void RequestQuit() = 0;

        /** Frames since the app started. */
        virtual Uint64 GetFrameIndex() const = 0;
    };

    // =============================================================================
    // EngineAutomation — the commands every app answers: the app itself (info, quit, screenshot),
    //   input (keys and mouse, fed to the engine as if pressed), the game's UI (its widgets, a click
    //   on one), the active world (its entities, their components, editing a component's values), and
    //   the player's save slots (SaveAutomationCommands.h). The editor adds its own, and replaces the
    //   edits with undoable ones.
    // =============================================================================
    namespace EngineAutomation
    {
        void Register(AutomationRunner& InRunner, IEngine& InEngine, IAutomationHost& InHost);

        /**
         * The entity InParams["entity"] names in InWorld: its id (Guid text) or its name (which must
         * be unique). Invalid, with OutError saying why, when there is none.
         */
        Entity FindEntity(World& InWorld, const nlohmann::json& InParams, OpaaxString& OutError);

        /**
         * An entity as JSON: {"id", "name", "parent", "map", "components": [names]}, or with
         * bInWithValues {"components": {name: values}}.
         */
        nlohmann::json DescribeEntity(Entity InEntity, const ComponentRegistry& InTypes, bool bInWithValues);

        /**
         * Whether every patch of InComponents, {type: values}, can be tried: each type known, each
         * value an object. Checked before an entity is made for them, so a request fails whole. False,
         * with OutError naming the first one that cannot and what to do.
         */
        bool CheckComponents(const ComponentRegistry& InTypes, const nlohmann::json& InComponents, OpaaxString& OutError);

        /**
         * InEntity's component InParams["type"] with InParams["value"] merged into its values
         * (a JSON merge patch: the fields given change, the others stay). Added when missing.
         * @return False, with OutError, for an unknown type, a value that is not an object, or a field
         *   of the wrong type; the component is then left as it was (not added when it was missing)
         */
        bool PatchComponent(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InParams,
                            OpaaxString& OutError);

        /**
         * Every patch of InComponents, {type: values}, applied to InEntity as PatchComponent does, or none:
         * when one fails (CheckComponents first, then a field of the wrong type) the ones before it are
         * put back. False, with OutError, then.
         */
        bool PatchComponents(Entity InEntity, const ComponentRegistry& InTypes, const nlohmann::json& InComponents,
                             OpaaxString& OutError);

        /**
         * Whether InActual meets what InParams expects: "equals" (numbers within 1e-4, objects and
         * arrays member by member), "near" within "tolerance" (default 0.01), "greater", "less",
         * "between" [min, max]. With none of them, any value passes. OutError tells what was found
         * when it does not.
         */
        bool CheckExpectation(const nlohmann::json& InActual, const nlohmann::json& InParams, OpaaxString& OutError);

        /**
         * InCanvas's widgets, depth-first: [{"name", "type", "visible", "x", "y", "width", "height"}],
         * x and y their centre, in the pixels of the canvas's target (origin top left). With InName,
         * only the widgets with that name.
         */
        nlohmann::json DescribeWidgets(const UICanvas& InCanvas, const OpaaxString& InName);

        /**
         * The pixel at the centre of InCanvas's widget named InName, where a click lands on it. False,
         * with OutError, when there is no such widget or it is hidden.
         */
        bool FindWidgetCentre(UICanvas& InCanvas, const OpaaxString& InName, Vector2F& OutPixel, OpaaxString& OutError);
    }
}
