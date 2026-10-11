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
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EntityOps.h"
#include "Editor/Operation/ExportOperations.h"
#include "Editor/Operation/MapOperations.h"
#include "Editor/Operation/NewLevelPlan.hpp"
#include "Editor/Export/GameExport.h"
#include "Editor/Undo/ComponentUndoables.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/UI/GuardedEditorDialogs.hpp"
#include "Engine/Registries/EngineRegistries.h"
#include "Platform/IFileSystem.h"
#include "World/Entity/Entity.h"
#include "World/Level.h"
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

        /** InPath made absolute: kept when absolute, else under the project's assets. */
        OpaaxString ToAbsolute(EditorContext& InContext, const std::string& InPath)
        {
            const std::filesystem::path lAsPath(InPath);
            return lAsPath.is_absolute() ? OpaaxString(lAsPath.generic_string().c_str())
                                         : InContext.Paths.AssetToAbsolute(OpaaxString(InPath.c_str()));
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

            OutPath = ToAbsolute(InContext, lPath);
            return true;
        }

        /**
         * A file a New command may write: in the project's assets, with InExtension, not there yet. False
         * (with OutError, naming InKey) otherwise.
         */
        bool NewFilePath(EditorContext& InContext, const OpaaxString& InPath, const char* InKey, const char* InExtension,
                         std::string& OutError)
        {
            const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InPath);
            if (lAssetRel.IsEmpty())
            {
                OutError = "\"" + std::string(InKey) + "\" is outside the project's assets: '" + InPath.CStr() + "'";
                return false;
            }
            if (!NewLevelPlan::HasExtension(lAssetRel, InExtension))
            {
                OutError = "\"" + std::string(InKey) + "\" names a " + InExtension + " file: '" + lAssetRel.CStr() + "'";
                return false;
            }
            if (InContext.FileSystem.IsPathExist(InPath))
            {
                OutError = "'" + std::string(lAssetRel.CStr()) + "' already exists: open it, or name another \"" + InKey + "\"";
                return false;
            }
            return true;
        }

        /** Fails a request that only makes sense while editing. */
        bool IsEditing(EditorContext& InContext, std::string& OutError)
        {
            const World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Edit)
            {
                OutError = "the game is playing: stop it first (editor.stop)";
                return false;
            }
            return true;
        }

        /**
         * Opens the level at InPath, losing unsaved changes when InParams["discard"] is true. False (with
         * OutError) when it did not open: refused, Play running, or a file that is not a level.
         */
        bool OpenLevel(EditorContext& InContext, const OpaaxString& InPath, const nlohmann::json& InParams,
                       std::string& OutError)
        {
            if (!IsEditing(InContext, OutError))
            {
                return false;
            }
            if (!InContext.FileSystem.IsPathExist(InPath))
            {
                OutError = "no file at '" + std::string(InPath.CStr()) + "'";
                return false;
            }

            InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext,
                                                    LevelPathParams{ InPath, InParams.value("discard", false) });
            if (!(InContext.LevelDocument.AbsPath() == InPath))
            {
                OutError = "the level did not open (see the log)";
                return false;
            }
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

        /** What a request does instead of a dialog it met, when its command has nothing better to say. */
        constexpr const char* ANSWER_ELSEWHERE = "use a command that takes the path or the answer (commands.list), "
                                                 "or write the file yourself (Docs/AssetFormats.md)";

        /** What a request that would lose unsaved changes does instead of asking. */
        constexpr const char* UNSAVED_CHANGES = "save them first (level.save), or pass \"discard\": true to lose them";

        /** Keeps the dialogs closed for its lifetime, and forgets what an earlier request left unanswered. */
        class UnattendedScope
        {
        public:
            explicit UnattendedScope(GuardedEditorDialogs& InDialogs) : m_Dialogs(InDialogs)
            {
                m_Dialogs.TakeUnanswered();
                m_Dialogs.SetUnattended(true);
            }

            ~UnattendedScope() { m_Dialogs.SetUnattended(false); }

            UnattendedScope(const UnattendedScope&)            = delete;
            UnattendedScope& operator=(const UnattendedScope&) = delete;

        private:
            GuardedEditorDialogs& m_Dialogs;
        };

        /**
         * Registers the editor's commands. Each runs with the dialogs closed: nobody is there to answer
         * them, and an OS dialog would hold the editor. A request that met one did nothing: it fails with
         * what was asked and what to do instead.
         */
        class Commands
        {
        public:
            Commands(AutomationRunner& InRunner, GuardedEditorDialogs& InDialogs) : m_Runner(InRunner), m_Dialogs(InDialogs) {}

            /** @param InInstead What to do rather than meet a dialog, for this command */
            void Register(const char* InName, const char* InHelp, AutomationHandler InHandler,
                          const char* InInstead = ANSWER_ELSEWHERE)
            {
                m_Runner.Register(InName, InHelp,
                    [&lDialogs = m_Dialogs, lHandler = Move(InHandler), InInstead](const nlohmann::json& InParams)
                    {
                        AutomationResult lResult;
                        {
                            UnattendedScope lScope(lDialogs);
                            lResult = lHandler(InParams);
                        }

                        const TDynArray<OpaaxString> lAsked = lDialogs.TakeUnanswered();
                        if (lAsked.empty())
                        {
                            return lResult;
                        }
                        return AutomationResult::Fail("the editor asked \"" + std::string(lAsked.front().CStr())
                                                      + "\", which a script cannot answer, so nothing was done: "
                                                      + InInstead);
                    });
            }

        private:
            AutomationRunner&     m_Runner;
            GuardedEditorDialogs& m_Dialogs;
        };

        // =========================================================================
        // Play In Editor, undo and plain commands
        // =========================================================================
        void RegisterEditor(Commands& InCommands, EditorContext& InContext)
        {
            const auto lCommand = [&InCommands, &InContext](const char* InName, const char* InHelp, const OpaaxTag& InTag)
            {
                InCommands.Register(InName, InHelp, [&InContext, InTag](const nlohmann::json&)
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

            InCommands.Register("editor.command",
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
        void RegisterFiles(Commands& InCommands, EditorContext& InContext)
        {
            InCommands.Register("project.export",
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

            InCommands.Register("level.open",
                "Opens the level at {path} (relative to the project's assets). Refused when the open level has "
                "unsaved changes, unless {discard} is true.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError) || !OpenLevel(InContext, lPath, InParams, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    return WorldState(InContext);
                },
                UNSAVED_CHANGES);

            // Replaces the engine's: the editor plays a level by opening it, then Play In Editor.
            InCommands.Register("level.play",
                "Opens the level at {path} (relative to the project's assets) and plays it in the editor; the next "
                "request runs once it plays. Refused when the open level has unsaved changes, unless {discard} is true.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError) || !OpenLevel(InContext, lPath, InParams, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    AutomationResult lResult = Dispatch(InContext, Tags::EDITOR_COMMAND_PLAY);
                    lResult.WaitFrames       = lResult.bOk ? 2u : 0u;
                    return lResult;
                },
                UNSAVED_CHANGES);

            InCommands.Register("map.open",
                "Opens the map at {path} (relative to the project's assets). A map of no open level replaces the "
                "level: refused when it has unsaved changes, unless {discard} is true.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_MAP_AT, InContext,
                                                            MapPathParams{ lPath, InParams.value("discard", false) });
                    return WorldState(InContext);
                },
                UNSAVED_CHANGES);

            InCommands.Register("level.new",
                "Creates a level at {path} with one new, empty map ({map}; default: Maps/<the level's name>.opaaxmap) "
                "and opens it for editing. Refuses files that exist; refused when the open level has unsaved changes, "
                "unless {discard} is true.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lLevel;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lLevel, lError) || !IsEditing(InContext, lError)
                        || !NewFilePath(InContext, lLevel, "path", LevelFile::LEVEL_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const std::string lMapParam = InParams.value("map", std::string());
                    const OpaaxString lMap      = lMapParam.empty()
                        ? InContext.Paths.AssetToAbsolute(NewLevelPlan::DefaultMap(InContext.Paths.AbsoluteToAsset(lLevel)))
                        : ToAbsolute(InContext, lMapParam);
                    if (!NewFilePath(InContext, lMap, "map", MapFile::MAP_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_NEW_LEVEL_AT, InContext,
                                                            NewLevelParams{ lLevel, lMap, InParams.value("discard", false) });
                    if (!(InContext.LevelDocument.AbsPath() == lLevel))
                    {
                        return AutomationResult::Fail("the level was not created (see the log)");
                    }

                    AutomationResult lResult = WorldState(InContext);
                    lResult.Value["level"]   = InContext.Paths.AbsoluteToAsset(lLevel).CStr();
                    lResult.Value["map"]     = InContext.Paths.AbsoluteToAsset(lMap).CStr();
                    return lResult;
                },
                UNSAVED_CHANGES);

            InCommands.Register("map.new",
                "Creates an empty map at {path}, adds it to the open level (whose file is saved at once) and focuses "
                "it: entities created next go there. Refuses a file that exists.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    std::string lError;
                    if (!AssetPath(InContext, InParams, lPath, lError) || !IsEditing(InContext, lError)
                        || !NewFilePath(InContext, lPath, "path", MapFile::MAP_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    if (!InContext.LevelDocument.HasLevel())
                    {
                        return AutomationResult::Fail("no level file is open to add the map to: open or create one "
                                                      "first (level.open, level.new)");
                    }

                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_NEW_MAP_AT, InContext, MapPathParams{ lPath });
                    if (!(InContext.MapDocument.GetMapId() == MapFile::StemId(lPath)))
                    {
                        return AutomationResult::Fail("the map was not created (see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "map", InContext.Paths.AbsoluteToAsset(lPath).CStr() } });
                });

            InCommands.Register("level.info",
                "The open level: its file ({level}; null for a map opened on its own), its maps ({id, path, "
                "persistent, focused, unsaved}) and whether anything is {unsaved}. Entities created next go to the "
                "focused map.",
                [&InContext](const nlohmann::json&)
                {
                    std::string lError;
                    if (!IsEditing(InContext, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    World* const lWorld = InContext.Worlds.GetActiveWorld();
                    Level* const lLevel = MapOps::ActiveLevel(InContext);
                    if (lLevel == nullptr)
                    {
                        return AutomationResult::Fail("no level is open: open or create one (level.open, level.new)");
                    }

                    const ComponentRegistry& lTypes      = InContext.Engine.GetRegistries().Components();
                    const MapId              lPersistent = lLevel->GetPersistentMapId();

                    nlohmann::json lMaps = nlohmann::json::array();
                    for (const Level::MountedMap& lMap : lLevel->GetMountedMaps())
                    {
                        lMaps.push_back(nlohmann::json{
                            { "id", lMap.Id.CStr() },
                            { "path", lMap.AssetRelPath.CStr() },
                            { "persistent", lMap.Id == lPersistent },
                            { "focused", lMap.Id == InContext.MapDocument.GetMapId() },
                            { "unsaved", InContext.LevelDocument.IsMapDirty(lMap.Id, *lWorld, lTypes) } });
                    }

                    const OpaaxString& lFile = InContext.LevelDocument.AbsPath();
                    return AutomationResult::Ok(nlohmann::json{
                        { "level", lFile.IsEmpty() ? nlohmann::json()
                                                   : nlohmann::json(InContext.Paths.AbsoluteToAsset(lFile).CStr()) },
                        { "maps", lMaps },
                        { "unsaved", InContext.LevelDocument.IsDirty(*lWorld, lTypes, *lLevel) } });
                });

            InCommands.Register("level.save",
                "Saves the open level's changed maps. With {all}: every map and the level file, rewritten in "
                "the current format (after a format change, or for files written by hand).",
                [&InContext](const nlohmann::json& InParams)
                {
                    return Dispatch(InContext, InParams.value("all", false) ? Tags::EDITOR_COMMAND_RESAVE_LEVEL
                                                                             : Tags::EDITOR_COMMAND_SAVE_LEVEL);
                });

            InCommands.Register("map.save", "Saves the focused map (it must have a file already).",
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
        void RegisterEntities(Commands& InCommands, EditorContext& InContext)
        {
            InCommands.Register("entity.create",
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

            InCommands.Register("entity.destroy", "Destroys {entity} (id or name), as Delete would. Undoable.",
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

            InCommands.Register("entity.select", "Selects {entities} (ids or names); an empty list clears the selection.",
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

            InCommands.Register("component.add", "Adds the component {type} to {entity}, with its defaults. Undoable.",
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

            InCommands.Register("component.remove", "Removes the component {type} from {entity}. Undoable.",
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

            InCommands.Register("entity.parent",
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

            InCommands.Register("entity.rename", "Renames {entity} to {name}. Undoable.",
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
            InCommands.Register("component.set",
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
        void RegisterPrefabs(Commands& InCommands, EditorContext& InContext)
        {
            InCommands.Register("prefab.create",
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

            InCommands.Register("prefab.place",
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

    void Register(AutomationRunner& InRunner, EditorContext& InContext, GuardedEditorDialogs& InDialogs)
    {
        Commands lCommands(InRunner, InDialogs);
        RegisterEditor(lCommands, InContext);
        RegisterFiles(lCommands, InContext);
        RegisterEntities(lCommands, InContext);
        RegisterPrefabs(lCommands, InContext);
    }
}
