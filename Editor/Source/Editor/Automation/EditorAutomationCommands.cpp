#include "Editor/Automation/EditorAutomationCommands.h"

#include <filesystem>
#include <string>

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Automation/AutomationRunner.h"
#include "Automation/EngineAutomationCommands.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/Commands/EditorNativeCommands.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EntityOps.h"
#include "Editor/Undo/ComponentUndoables.h"
#include "Editor/Undo/EditorUndo.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor::EditorAutomation
{
    namespace
    {
        World* ActiveWorld(EditorContext& InContext, std::string& OutError)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr)
            {
                OutError = "no active world";
            }
            return lWorld;
        }

        /** The active world's mode after a command, so the answer shows what it did. */
        AutomationResult WorldState(EditorContext& InContext)
        {
            const World* lWorld = InContext.Worlds.GetActiveWorld();
            return AutomationResult::Ok(nlohmann::json{
                { "world",  lWorld != nullptr ? nlohmann::json(lWorld->GetName().CStr()) : nlohmann::json() },
                { "mode",   lWorld != nullptr ? nlohmann::json(ToString(lWorld->GetMode())) : nlohmann::json() },
                { "paused", InContext.Worlds.IsPaused() } });
        }

        /** Runs a command that takes no arguments, as a menu entry or a key would. */
        AutomationResult Dispatch(EditorContext& InContext, const OpaaxTag& InTag)
        {
            if (!InContext.Extensions.Commands().Execute(InTag, InContext))
            {
                return AutomationResult::Fail("the command '" + std::string(InTag.ToString().CStr())
                                              + "' does not exist or takes arguments (see the log)");
            }
            return WorldState(InContext);
        }

        /** An absolute path from InParams["path"]: kept when absolute, else under the project's assets. */
        bool AssetPath(EditorContext& InContext, const nlohmann::json& InParams, OpaaxString& OutPath, std::string& OutError)
        {
            const std::string lPath = InParams.value("path", std::string());
            if (lPath.empty())
            {
                OutError = "\"path\" names the file (absolute, or relative to the project's assets)";
                return false;
            }

            const std::filesystem::path lAsPath(lPath);
            OutPath = lAsPath.is_absolute() ? OpaaxString(lAsPath.generic_string().c_str())
                                            : InContext.Paths.AssetToAbsolute(OpaaxString(lPath.c_str()));
            return true;
        }

        /** InParams["entity"] in the active world. */
        Entity FindEntity(EditorContext& InContext, const nlohmann::json& InParams, std::string& OutError)
        {
            World* const lWorld = ActiveWorld(InContext, OutError);
            return (lWorld != nullptr) ? EngineAutomation::FindEntity(*lWorld, InParams, OutError) : Entity{};
        }

        /**
         * Changes the component InParams describes. In the edit world it is an undo step, as an
         * Inspector edit; in Play the clone is changed directly.
         */
        bool EditComponent(EditorContext& InContext, Entity InEntity, const nlohmann::json& InParams, std::string& OutError)
        {
            const ComponentRegistry& lTypes = InContext.Engine.GetRegistries().Components();

            if (InEntity.GetWorld()->GetMode() != EWorldMode::Edit)
            {
                return EngineAutomation::PatchComponent(InEntity, lTypes, InParams, OutError);
            }

            EntityComponentsEdit lEdit;
            lEdit.Begin(InContext, InEntity, EUndoWorld::Active);
            if (!EngineAutomation::PatchComponent(InEntity, lTypes, InParams, OutError))
            {
                return false;
            }

            if (lEdit.End(InContext))
            {
                InContext.Undo.Record(Move(lEdit));
            }
            InEntity.GetWorld()->MarkChanged();
            return true;
        }

        nlohmann::json Describe(EditorContext& InContext, Entity InEntity)
        {
            return EngineAutomation::DescribeEntity(InEntity, InContext.Engine.GetRegistries().Components(), true);
        }

        // =========================================================================
        // Play In Editor, undo and plain commands
        // =========================================================================
        void RegisterEditor(AutomationRunner& InRunner, EditorContext& InContext)
        {
            const auto lCommand = [&InRunner, &InContext](const char* InName, const char* InHelp, const OpaaxTag& InTag)
            {
                InRunner.Register(InName, InHelp, [&InContext, InTag](const nlohmann::json&)
                {
                    return Dispatch(InContext, InTag);
                });
            };

            lCommand("editor.play",  "Plays the edit world in the editor (Play In Editor).", Tags::EDITOR_COMMAND_PLAY);
            lCommand("editor.stop",  "Stops playing and returns to the edit world.",         Tags::EDITOR_COMMAND_STOP);
            lCommand("editor.pause", "Pauses the game, or resumes it.",                      Tags::EDITOR_COMMAND_TOGGLE_PAUSE);
            lCommand("editor.step",  "Runs one frame of a paused game.",                     Tags::EDITOR_COMMAND_STEP);
            lCommand("editor.undo",  "Undoes the last edit.",                                Tags::EDITOR_COMMAND_UNDO);
            lCommand("editor.redo",  "Redoes the last undone edit.",                         Tags::EDITOR_COMMAND_REDO);
            lCommand("level.save",   "Saves the open level.",                                Tags::EDITOR_COMMAND_SAVE_LEVEL);

            InRunner.Register("editor.command",
                "Runs an editor command that takes no arguments by its {tag} (\"Editor.Command.FocusSelected\"...).",
                [&InContext](const nlohmann::json& InParams)
                {
                    const std::string lTag = InParams.value("tag", std::string());
                    if (!OpaaxTag::IsValidTagText(OpaaxStringView(lTag.c_str())))
                    {
                        return AutomationResult::Fail("\"tag\" is a command tag, like Editor.Command.Undo");
                    }
                    return Dispatch(InContext, OpaaxTag(lTag.c_str()));
                });
        }

        // =========================================================================
        // Levels and maps
        // =========================================================================
        void RegisterFiles(AutomationRunner& InRunner, EditorContext& InContext)
        {
            InRunner.Register("level.open", "Opens the level at {path} (relative to the project's assets).",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext, LevelPathParams{ lPath });
                    return WorldState(InContext);
                });

            InRunner.Register("map.open", "Opens the map at {path} (relative to the project's assets).",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_MAP_AT, InContext, MapPathParams{ lPath });
                    return WorldState(InContext);
                });

            InRunner.Register("map.save", "Saves the focused map (it must have a file already).",
                [&InContext](const nlohmann::json&)
                {
                    // Without a file it would ask where, and nobody would answer.
                    if (!InContext.MapDocument.HasMap())
                    {
                        return AutomationResult::Fail("the focused map has no file yet: save it once from the editor");
                    }
                    return Dispatch(InContext, Tags::EDITOR_COMMAND_SAVE_MAP);
                });
        }

        // =========================================================================
        // Entities and components
        // =========================================================================
        void RegisterEntities(AutomationRunner& InRunner, EditorContext& InContext)
        {
            InRunner.Register("entity.create",
                "Creates an entity in the edit world: {name}, {map} (default: the focused one), {components}: "
                "{type: values}. Undoable; returns the entity.",
                [&InContext](const nlohmann::json& InParams)
                {
                    const std::string lName = InParams.value("name", std::string("Entity"));
                    const std::string lMap  = InParams.value("map", std::string());
                    const MapId       lOwner = lMap.empty() ? InContext.MapDocument.GetMapId() : MapId(lMap.c_str());

                    Entity lEntity = EntityOps::Create(InContext, lOwner, OpaaxString(lName.c_str()));
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail("the entity was not created (Play running, or no map: see the log)");
                    }

                    const auto lComponents = InParams.find("components");
                    if (lComponents != InParams.end() && lComponents->is_object())
                    {
                        for (const auto& [lType, lValue] : lComponents->items())
                        {
                            std::string lError;
                            if (!EditComponent(InContext, lEntity, nlohmann::json{ { "type", lType }, { "value", lValue } }, lError))
                            {
                                return AutomationResult::Fail(lError);
                            }
                        }
                    }

                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });

            InRunner.Register("entity.destroy", "Destroys {entity} (id or name), as Delete would. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InContext.Selection.Select(lEntity);
                    EntityOps::DestroySelected(InContext);
                    return AutomationResult::Ok();
                });

            InRunner.Register("entity.select", "Selects {entities} (ids or names); an empty list clears the selection.",
                [&InContext](const nlohmann::json& InParams)
                {
                    InContext.Selection.Clear();

                    nlohmann::json lSelected = nlohmann::json::array();
                    for (const nlohmann::json& lRef : InParams.value("entities", nlohmann::json::array()))
                    {
                        std::string  lError;
                        const Entity lEntity = FindEntity(InContext, nlohmann::json{ { "entity", lRef } }, lError);
                        if (!lEntity.IsValid())
                        {
                            return AutomationResult::Fail(lError);
                        }
                        InContext.Selection.Add(lEntity);
                        lSelected.push_back(Describe(InContext, lEntity).at("id"));
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "selected", lSelected } });
                });

            InRunner.Register("component.add", "Adds the component {type} to {entity}, with its defaults. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const std::string lType = InParams.value("type", std::string());
                    if (!EntityOps::AddComponent(InContext, lEntity, OpaaxStringID(lType.c_str())))
                    {
                        return AutomationResult::Fail("'" + lType + "' was not added (unknown, or already there)");
                    }
                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });

            InRunner.Register("component.remove", "Removes the component {type} from {entity}. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const std::string lType = InParams.value("type", std::string());
                    if (!EntityOps::RemoveComponent(InContext, lEntity, OpaaxStringID(lType.c_str())))
                    {
                        return AutomationResult::Fail("'" + lType + "' was not removed (absent, or every entity needs it)");
                    }
                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });

            // Replaces the engine's: in the edit world, an edit is an undo step.
            InRunner.Register("component.set",
                "Changes fields of {entity}'s component {type}: {value} is merged into it (added when missing). "
                "Undoable in the edit world.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid() || !EditComponent(InContext, lEntity, InParams, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });
        }
    }

    void Register(AutomationRunner& InRunner, EditorContext& InContext)
    {
        RegisterEditor(InRunner, InContext);
        RegisterFiles(InRunner, InContext);
        RegisterEntities(InRunner, InContext);
    }
}
