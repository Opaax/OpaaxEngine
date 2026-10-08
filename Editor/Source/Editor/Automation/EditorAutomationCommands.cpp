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
#include "Editor/Operation/ExportOperations.h"
#include "Editor/Export/GameExport.h"
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
            InRunner.Register("project.export",
                "Exports the game into the folder {path}: its release build, content and the engine's, ready to run "
                "elsewhere. Answered when done (it takes minutes), with the output's log.",
                [&InContext](const nlohmann::json& InParams)
                {
                    const std::string lPath = InParams.value("path", std::string());
                    if (lPath.empty() || !ExportOps::Start(InContext, std::filesystem::absolute(lPath).generic_string()))
                    {
                        return AutomationResult::Fail("the export did not start (see the log): a folder is needed, and "
                                                      "one export at a time");
                    }

                    AutomationResult lResult;
                    lResult.WaitUntil = [&InContext]() { return !InContext.Export.IsRunning(); };
                    lResult.Answer    = [&InContext]()
                    {
                        const GameExport& lExport = InContext.Export;
                        if (lExport.GetState() != GameExport::EState::Succeeded)
                        {
                            return AutomationResult::Fail("the export failed at '" + lExport.GetFailedStep()
                                                          + "': see " + lExport.GetLog());
                        }
                        return AutomationResult::Ok(nlohmann::json{ { "path", lExport.GetDestination() },
                                                                    { "log", lExport.GetLog() } });
                    };
                    return lResult;
                });

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

            // Replaces the engine's: the editor plays a level by opening it, then Play In Editor.
            InRunner.Register("level.play",
                "Opens the level at {path} (relative to the project's assets) and plays it in the editor; the next "
                "request runs once it plays.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext, LevelPathParams{ lPath });

                    AutomationResult lResult = Dispatch(InContext, Tags::EDITOR_COMMAND_PLAY);
                    lResult.WaitFrames       = lResult.bOk ? 2u : 0u;
                    return lResult;
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

            InRunner.Register("level.save",
                "Saves the open level's changed maps. With {all}: every map and the level file, rewritten in "
                "the current format (after a format change, or for files written by hand).",
                [&InContext](const nlohmann::json& InParams)
                {
                    return Dispatch(InContext, InParams.value("all", false) ? Tags::EDITOR_COMMAND_RESAVE_LEVEL
                                                                             : Tags::EDITOR_COMMAND_SAVE_LEVEL);
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

            InRunner.Register("entity.parent",
                "Puts {entity} (and its children) under {parent} (id or name; empty or absent detaches it), "
                "keeping where it is in the world. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lChild = FindEntity(InContext, InParams, lError);
                    if (!lChild.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    EntityID lParent = ENTITY_NONE;
                    if (const std::string lRef = InParams.value("parent", std::string()); !lRef.empty())
                    {
                        const Entity lFound = FindEntity(InContext, nlohmann::json{ { "entity", lRef } }, lError);
                        if (!lFound.IsValid())
                        {
                            return AutomationResult::Fail(lError);
                        }
                        lParent = lFound.GetHandle();
                    }

                    if (!EntityOps::Reparent(InContext, EUndoWorld::Active, lChild.GetHandle(), lParent))
                    {
                        return AutomationResult::Fail("nothing changed (Play running, already there, or a parent "
                                                      "inside its own children: see the log)");
                    }
                    return AutomationResult::Ok(Describe(InContext, lChild));
                });

            InRunner.Register("entity.rename", "Renames {entity} to {name}. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    std::string  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    const std::string lName = InParams.value("name", std::string());
                    if (!lEntity.IsValid() || lName.empty())
                    {
                        return AutomationResult::Fail(lEntity.IsValid() ? "\"name\" is the new name" : lError);
                    }

                    EntityOps::Rename(InContext, lEntity, OpaaxString(lName.c_str()));
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

        // =========================================================================
        // Prefabs
        // =========================================================================
        void RegisterPrefabs(AutomationRunner& InRunner, EditorContext& InContext)
        {
            InRunner.Register("prefab.create",
                "Writes {entities} (ids or names, with their children) as a prefab at {path}, then replaces them "
                "with an instance of it, as Create Prefab does. The file is written at once, outside undo.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InContext.Selection.Clear();
                    for (const nlohmann::json& lRef : InParams.value("entities", nlohmann::json::array()))
                    {
                        const Entity lEntity = FindEntity(InContext, nlohmann::json{ { "entity", lRef } }, lError);
                        if (!lEntity.IsValid())
                        {
                            return AutomationResult::Fail(lError);
                        }
                        InContext.Selection.Add(lEntity);
                    }

                    if (InContext.Selection.Count() == 0)
                    {
                        return AutomationResult::Fail("\"entities\" lists what goes in the prefab");
                    }
                    if (!EntityOps::CreatePrefabFromSelection(InContext, lPath))
                    {
                        return AutomationResult::Fail("the prefab was not created (Play running, or a path outside the "
                                                      "assets: see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "path", lPath.CStr() } });
                });

            InRunner.Register("prefab.place",
                "Places an instance of the prefab at {path} in the focused map, its first entity at {position} "
                "({x, y}; default: where the prefab has it). Undoable; returns the placed entities.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    Vector2F        lAt     = { 0.f, 0.f };
                    const Vector2F* lAtPtr  = nullptr;
                    if (const auto lPosition = InParams.find("position"); lPosition != InParams.end() && lPosition->is_object())
                    {
                        lAt    = Vector2F{ lPosition->value("x", 0.f), lPosition->value("y", 0.f) };
                        lAtPtr = &lAt;
                    }

                    if (EntityOps::InstantiatePrefab(InContext, lPath, InContext.MapDocument.GetMapId(), lAtPtr) == 0)
                    {
                        return AutomationResult::Fail("nothing was placed (Play running, no map, or the prefab did not "
                                                      "load: see the log)");
                    }

                    // The placed entities are the selection.
                    World* const   lWorld  = InContext.Worlds.GetActiveWorld();
                    nlohmann::json lPlaced = nlohmann::json::array();
                    for (const EntityID lId : InContext.Selection.Ids())
                    {
                        const Entity lEntity{ lId, lWorld };
                        lPlaced.push_back(nlohmann::json{ { "id", Describe(InContext, lEntity).at("id") },
                                                          { "name", lEntity.GetName().CStr() } });
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "entities", lPlaced } });
                });
        }
    }

    void Register(AutomationRunner& InRunner, EditorContext& InContext)
    {
        RegisterEditor(InRunner, InContext);
        RegisterFiles(InRunner, InContext);
        RegisterEntities(InRunner, InContext);
        RegisterPrefabs(InRunner, InContext);
    }
}
