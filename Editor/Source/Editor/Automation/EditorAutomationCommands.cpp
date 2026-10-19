#include "Editor/Automation/EditorAutomationCommands.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Automation/AutomationRunner.h"
#include "Automation/EngineAutomationCommands.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/Assets/AssetEditors.h"
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
#include "Editor/Prefab/EditorPrefabDocument.h"
#include "Editor/Undo/ComponentUndoables.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/UI/GuardedEditorDialogs.hpp"
#include "Engine/Registries/EngineRegistries.h"
#include "Platform/IFileSystem.h"
#include "Resources/Asset/AssetFile.h"
#include "UI/UICanvasFile.h"
#include "World/Entity/Entity.h"
#include "World/Level.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor::EditorAutomation
{
    namespace
    {
        World* ActiveWorld(EditorContext& InContext, OpaaxString& OutError)
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
                return AutomationResult::Fail("the command '" + InTag.ToString()
                                              + "' does not exist or takes arguments (see the log)");
            }
            return WorldState(InContext);
        }

        /** InPath made absolute: kept when absolute, else under the project's assets. */
        OpaaxString ToAbsolute(EditorContext& InContext, const OpaaxString& InPath)
        {
            return PathString::IsAbsolute(InPath) ? PathString::Generic(InPath)
                                                  : InContext.Paths.AssetToAbsolute(InPath);
        }

        /** An absolute path from InParams["path"]: kept when absolute, else under the project's assets. */
        bool AssetPath(EditorContext& InContext, const nlohmann::json& InParams, OpaaxString& OutPath, OpaaxString& OutError)
        {
            const OpaaxString lPath = InParams.value("path", OpaaxString());
            if (lPath.IsEmpty())
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
                         OpaaxString& OutError)
        {
            const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InPath);
            if (lAssetRel.IsEmpty())
            {
                OutError = OpaaxString::Format("\"{}\" is outside the project's assets: '{}'", InKey, InPath);
                return false;
            }
            if (!NewLevelPlan::HasExtension(lAssetRel, InExtension))
            {
                OutError = OpaaxString::Format("\"{}\" names a {} file: '{}'", InKey, InExtension, lAssetRel);
                return false;
            }
            if (InContext.FileSystem.IsPathExist(InPath))
            {
                OutError = OpaaxString::Format("'{}' already exists: open it, or name another \"{}\"", lAssetRel, InKey);
                return false;
            }
            return true;
        }

        /** Fails a request that only makes sense while editing. */
        bool IsEditing(EditorContext& InContext, OpaaxString& OutError)
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
                       OpaaxString& OutError)
        {
            if (!IsEditing(InContext, OutError))
            {
                return false;
            }
            if (!InContext.FileSystem.IsPathExist(InPath))
            {
                OutError = "no file at '" + InPath + "'";
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
        Entity FindEntity(EditorContext& InContext, const nlohmann::json& InParams, OpaaxString& OutError)
        {
            World* const lWorld = ActiveWorld(InContext, OutError);
            return (lWorld != nullptr) ? EngineAutomation::FindEntity(*lWorld, InParams, OutError) : Entity{};
        }

        /**
         * Changes the component InParams describes. In the edit world it is an undo step, as an
         * Inspector edit; in Play the clone is changed directly.
         */
        bool EditComponent(EditorContext& InContext, Entity InEntity, const nlohmann::json& InParams, OpaaxString& OutError)
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
                        return AutomationResult::Fail("the editor asked \"" + lAsked.front()
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
                    const OpaaxString lTag = InParams.value("tag", OpaaxString());
                    if (!OpaaxTag::IsValidTagText(lTag))
                    {
                        return AutomationResult::Fail("\"tag\" is a command tag, like Editor.Command.Undo");
                    }
                    return Dispatch(InContext, OpaaxTag(lTag));
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
                    const OpaaxString lPath = InParams.value("path", OpaaxString());
                    if (lPath.IsEmpty() || !ExportOps::Start(InContext, FileSystem::MakeAbsolute(lPath)))
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
                    OpaaxString lError;
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
                    OpaaxString lError;
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
                    OpaaxString lError;
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
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lLevel, lError) || !IsEditing(InContext, lError)
                        || !NewFilePath(InContext, lLevel, "path", LevelFile::LEVEL_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString lMapParam = InParams.value("map", OpaaxString());
                    const OpaaxString lMap      = lMapParam.IsEmpty()
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
                    OpaaxString lError;
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
                    OpaaxString lError;
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

            InCommands.Register("map.save",
                "Saves the focused map. With {path}: writes it to that new file instead, which the level names from "
                "then on (its file is saved at once); the old file stays as it was last saved.",
                [&InContext](const nlohmann::json& InParams)
                {
                    // Without a focused map Save would ask where, and nobody would answer.
                    if (!InContext.MapDocument.HasMap())
                    {
                        return AutomationResult::Fail("no map is focused: open a level or a map first (level.open, "
                                                      "map.open)");
                    }
                    if (!InParams.contains("path"))
                    {
                        return Dispatch(InContext, Tags::EDITOR_COMMAND_SAVE_MAP);
                    }

                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError) || !IsEditing(InContext, lError)
                        || !NewFilePath(InContext, lPath, "path", MapFile::MAP_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_MAP_AS_AT, InContext,
                                                            MapPathParams{ lPath });
                    if (!(InContext.MapDocument.AbsPath() == lPath))
                    {
                        return AutomationResult::Fail("the map was not written there (see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "map", InContext.Paths.AbsoluteToAsset(lPath).CStr() } });
                });

            InCommands.Register("map.add",
                "Adds the existing map {path} to the open level (whose file is saved at once): its entities load. "
                "Answers its {map} and map {id}; map.open focuses it.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError) || !IsEditing(InContext, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    if (!InContext.LevelDocument.HasLevel())
                    {
                        return AutomationResult::Fail("no level file is open to add the map to: open or create one "
                                                      "first (level.open, level.new)");
                    }
                    if (!InContext.FileSystem.IsPathExist(lPath))
                    {
                        return AutomationResult::Fail("no file at '" + lPath + "': map.new creates a map");
                    }

                    const OpaaxString  lAssetRel = InContext.Paths.AbsoluteToAsset(lPath);
                    const Level* const lLevel    = MapOps::ActiveLevel(InContext);
                    if (lAssetRel.IsEmpty())
                    {
                        return AutomationResult::Fail("a level only names maps of the project's assets");
                    }
                    if (lLevel == nullptr)
                    {
                        return AutomationResult::Fail("no level is open (see the log)");
                    }

                    // Each reason a level refuses the map, said before trying.
                    for (const OpaaxString& lListed : lLevel->GetData().Maps)
                    {
                        if (lListed == lAssetRel)
                        {
                            return AutomationResult::Fail("'" + lAssetRel + "' is in the level already");
                        }
                    }
                    MapData lData;
                    if (!MapFile::Load(lPath, lData))
                    {
                        return AutomationResult::Fail("'" + lAssetRel + "' is not a map that can be read (see the log)");
                    }
                    for (const Level::MountedMap& lMounted : lLevel->GetMountedMaps())
                    {
                        if (lMounted.Id == lData.Id)
                        {
                            return AutomationResult::Fail("'" + lAssetRel + "' is map '" + lData.Id.CStr()
                                                          + "', which the level has already (in '"
                                                          + lMounted.AssetRelPath.CStr() + "'): a level holds each map once");
                        }
                    }

                    const Uint64 lBefore = lLevel->GetData().MapCount();
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_ADD_MAP_TO_LEVEL_AT, InContext,
                                                            MapPathParams{ lPath });
                    if (lLevel->GetData().MapCount() == lBefore)
                    {
                        return AutomationResult::Fail("the map was not added (see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "map", lAssetRel },
                                                                { "id", lLevel->GetMountedMaps().back().Id.CStr() } });
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
                    const OpaaxString lName = InParams.value("name", OpaaxString("Entity"));
                    const OpaaxString lMap  = InParams.value("map", OpaaxString());
                    const MapId       lOwner = lMap.IsEmpty() ? InContext.MapDocument.GetMapId() : MapId(lMap);

                    Entity lEntity = EntityOps::Create(InContext, lOwner, lName);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail("the entity was not created (Play running, or no map: see the log)");
                    }

                    const auto lComponents = InParams.find("components");
                    if (lComponents != InParams.end() && lComponents->is_object())
                    {
                        for (const auto& [lType, lValue] : lComponents->items())
                        {
                            OpaaxString lError;
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
                    OpaaxString  lError;
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
                        OpaaxString  lError;
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
                    OpaaxString  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString lType = InParams.value("type", OpaaxString());
                    if (!EntityOps::AddComponent(InContext, lEntity, OpaaxStringID(lType)))
                    {
                        return AutomationResult::Fail("'" + lType + "' was not added (unknown, or already there)");
                    }
                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });

            InCommands.Register("component.remove", "Removes the component {type} from {entity}. Undoable.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    if (!lEntity.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString lType = InParams.value("type", OpaaxString());
                    if (!EntityOps::RemoveComponent(InContext, lEntity, OpaaxStringID(lType)))
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
                    OpaaxString  lError;
                    const Entity lChild = FindEntity(InContext, InParams, lError);
                    if (!lChild.IsValid())
                    {
                        return AutomationResult::Fail(lError);
                    }

                    EntityID lParent = ENTITY_NONE;
                    if (const OpaaxString lRef = InParams.value("parent", OpaaxString()); !lRef.IsEmpty())
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
                    OpaaxString  lError;
                    const Entity lEntity = FindEntity(InContext, InParams, lError);
                    const OpaaxString lName = InParams.value("name", OpaaxString());
                    if (!lEntity.IsValid() || lName.IsEmpty())
                    {
                        return AutomationResult::Fail(lEntity.IsValid() ? "\"name\" is the new name" : lError);
                    }

                    EntityOps::Rename(InContext, lEntity, lName);
                    return AutomationResult::Ok(Describe(InContext, lEntity));
                });

            // Replaces the engine's: in the edit world, an edit is an undo step.
            InCommands.Register("component.set",
                "Changes fields of {entity}'s component {type}: {value} is merged into it (added when missing). "
                "Undoable in the edit world.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString  lError;
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
                    OpaaxString lError;
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
                    OpaaxString lError;
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

            InCommands.Register("prefab.variant",
                "Writes a variant of the prefab {prefab} at {path}: it inherits everything from it, except "
                "{components} ({type: values}) changed on its {entity} (name or id). Opens it in the Prefab panel; "
                "refuses a file that exists.",
                [&InContext](const nlohmann::json& InParams)
                {
                    const OpaaxString lBaseParam = InParams.value("prefab", OpaaxString());
                    if (lBaseParam.IsEmpty())
                    {
                        return AutomationResult::Fail("\"prefab\" names the prefab the variant is made from");
                    }

                    const OpaaxString lBase = ToAbsolute(InContext, lBaseParam);
                    OpaaxString       lPath;
                    OpaaxString       lError;
                    if (!AssetPath(InContext, InParams, lPath, lError)
                        || !NewFilePath(InContext, lPath, "path", PrefabFile::PREFAB_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    if (!InContext.FileSystem.IsPathExist(lBase))
                    {
                        return AutomationResult::Fail("no prefab at '" + lBase + "'");
                    }

                    EditorPrefabDocument& lDocument = InContext.PrefabDocument;
                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_PREFAB_AT, InContext,
                                                            PrefabPathParams{ lBase });
                    if (!(lDocument.AbsPath() == lBase) || lDocument.GetWorld() == nullptr)
                    {
                        return AutomationResult::Fail("the prefab did not open (see the log)");
                    }

                    // The changes are made in the prefab's own world: saved as a variant, they are its overrides.
                    const auto lComponents = InParams.find("components");
                    if (lComponents != InParams.end() && lComponents->is_object() && !lComponents->empty())
                    {
                        const Entity lEntity = EngineAutomation::FindEntity(*lDocument.GetWorld(), InParams, lError);
                        if (!lEntity.IsValid())
                        {
                            return AutomationResult::Fail("\"entity\" names the prefab's entity to change: " + lError);
                        }

                        const ComponentRegistry& lTypes = InContext.Engine.GetRegistries().Components();
                        for (const auto& [lType, lValue] : lComponents->items())
                        {
                            const nlohmann::json lPatch{ { "type", lType }, { "value", lValue } };
                            if (!EngineAutomation::PatchComponent(lEntity, lTypes, lPatch, lError))
                            {
                                return AutomationResult::Fail(lError);
                            }
                        }
                    }

                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_PREFAB_AS_VARIANT_AT, InContext,
                                                            PrefabPathParams{ lPath });
                    if (!(lDocument.AbsPath() == lPath))
                    {
                        return AutomationResult::Fail("the variant was not written (see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "path", InContext.Paths.AbsoluteToAsset(lPath).CStr() } });
                });
        }

        // =========================================================================
        // UI canvases
        // =========================================================================
        void RegisterUI(Commands& InCommands, EditorContext& InContext)
        {
            InCommands.Register("ui.new",
                "Creates an empty UI canvas at {path} (.opaaxui: a root panel at the project's reference height) "
                "and opens it in the UI panel. Its widgets are written in the file (Docs/AssetFormats.md). Refuses "
                "a file that exists.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError)
                        || !NewFilePath(InContext, lPath, "path", UICanvasFile::UI_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_NEW_UI_AT, InContext,
                                                            UICanvasPathParams{ lPath });
                    if (!InContext.FileSystem.IsPathExist(lPath))
                    {
                        return AutomationResult::Fail("the canvas was not written (see the log)");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "path", InContext.Paths.AbsoluteToAsset(lPath).CStr() } });
                });
        }

        // =========================================================================
        // Assets
        // =========================================================================

        /** What an open asset shows: its file, class, values, and whether they are saved. */
        nlohmann::json DescribeAsset(EditorContext& InContext, const AssetDocument& InDocument)
        {
            const EditorUndo* lHistory = InContext.Assets.FindHistory(InDocument.GetAbsPath());

            return nlohmann::json{
                { "path",       InContext.Paths.AbsoluteToAsset(InDocument.GetAbsPath()).CStr() },
                { "class",      InDocument.GetClassName().CStr() },
                { "open",       true },
                { "known",      InDocument.IsClassKnown() },
                { "unsaved",    InDocument.IsDirty() },
                { "canUndo",    lHistory != nullptr && lHistory->CanUndo() },
                { "canRedo",    lHistory != nullptr && lHistory->CanRedo() },
                { "properties", InDocument.GetProperties() } };
        }

        /** InParams["path"] opened in its window. Null (with OutError) when it is not an asset that opens. */
        AssetDocument* OpenAsset(EditorContext& InContext, const nlohmann::json& InParams, OpaaxString& OutError)
        {
            OpaaxString lPath;
            if (!AssetPath(InContext, InParams, lPath, OutError))
            {
                return nullptr;
            }
            if (!InContext.FileSystem.IsPathExist(lPath))
            {
                OutError = "no file at '" + lPath + "'";
                return nullptr;
            }
            if (!InContext.Assets.Open(lPath, &OutError))
            {
                return nullptr;
            }
            return InContext.Assets.Find(lPath);
        }

        void RegisterAssets(Commands& InCommands, EditorContext& InContext)
        {
            InCommands.Register("asset.classes",
                "The asset classes a project can make: {classes: [{name, label, category, properties}]}, each with "
                "its properties at their default values.",
                [&InContext](const nlohmann::json&)
                {
                    const ObjectClassRegistry& lClasses = InContext.Engine.GetRegistries().ObjectClasses();
                    const AssetTypeRegistry&   lTypes   = InContext.Extensions.AssetTypes();

                    nlohmann::json lList = nlohmann::json::array();
                    for (const ObjectClass* lClass : lClasses.GetClassesOf(Asset::StaticClass(), /*bInConcreteOnly*/ true))
                    {
                        lList.push_back(nlohmann::json{
                            { "name",       lClass->GetName().CStr() },
                            { "label",      lTypes.LabelOf(*lClass).CStr() },
                            { "category",   lTypes.CategoryOf(*lClass).CStr() },
                            { "properties", lClass->WriteProperties(*lClass->GetDefaultObject()) } });
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "classes", lList } });
                });

            InCommands.Register("asset.new",
                "Creates an asset of {class} at {path} (.opaaxasset), every field at its default, then sets "
                "{properties} if given, saves and opens it. Refuses a file that exists.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError)
                        || !NewFilePath(InContext, lPath, "path", AssetFile::ASSET_EXTENSION, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const OpaaxString  lClassName = InParams.value("class", OpaaxString());
                    const ObjectClass* lClass =
                        InContext.Engine.GetRegistries().ObjectClasses().FindByName(OpaaxStringID::Find(lClassName));
                    if (lClass == nullptr)
                    {
                        return AutomationResult::Fail("\"class\" names a registered asset class (asset.classes lists them): '"
                                                      + lClassName + "'");
                    }

                    if (!InContext.Assets.Create(lPath, *lClass, &lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    if (const auto lProperties = InParams.find("properties"); lProperties != InParams.end())
                    {
                        if (!InContext.Assets.Edit(lPath, *lProperties, &lError) || !InContext.Assets.Save(lPath))
                        {
                            return AutomationResult::Fail("created, but its properties were refused: " + lError);
                        }
                    }

                    return AutomationResult::Ok(DescribeAsset(InContext, *InContext.Assets.Find(lPath)));
                });

            InCommands.Register("asset.open",
                "Opens the asset at {path} in a window of its own (or brings its window forward): {path, class, "
                "properties, unsaved, canUndo, canRedo}.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString          lError;
                    const AssetDocument* lDocument = OpenAsset(InContext, InParams, lError);
                    return lDocument != nullptr ? AutomationResult::Ok(DescribeAsset(InContext, *lDocument))
                                                : AutomationResult::Fail(lError);
                });

            InCommands.Register("asset.get",
                "The asset at {path}: its class and values. Those of its window when it is open (unsaved changes "
                "included), else those of its file.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    if (const AssetDocument* lOpen = InContext.Assets.Find(lPath))
                    {
                        return AutomationResult::Ok(DescribeAsset(InContext, *lOpen));
                    }

                    AssetFile::Contents lContents;
                    if (!AssetFile::Load(lPath, lContents))
                    {
                        return AutomationResult::Fail("no asset file at '" + lPath + "' (see the log)");
                    }

                    return AutomationResult::Ok(nlohmann::json{
                        { "path",       InContext.Paths.AbsoluteToAsset(lPath).CStr() },
                        { "class",      lContents.Class.CStr() },
                        { "open",       false },
                        { "properties", lContents.Properties } });
                });

            InCommands.Register("asset.set",
                "Changes the asset at {path}: {properties} merged into its values (opened first if needed), as one "
                "undo step, not saved (asset.save). A value of the wrong type changes nothing.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString          lError;
                    const AssetDocument* lDocument = OpenAsset(InContext, InParams, lError);
                    if (lDocument == nullptr)
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const auto lProperties = InParams.find("properties");
                    if (lProperties == InParams.end() || !lProperties->is_object())
                    {
                        return AutomationResult::Fail("\"properties\" is an object of the values to change");
                    }

                    if (!InContext.Assets.Edit(lDocument->GetAbsPath(), *lProperties, &lError))
                    {
                        return AutomationResult::Fail(lError);
                    }
                    return AutomationResult::Ok(DescribeAsset(InContext, *lDocument));
                });

            const auto lStep = [&InCommands, &InContext](const char* InName, const char* InHelp, const bool bInUndo)
            {
                InCommands.Register(InName, InHelp, [&InContext, bInUndo](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const AssetDocument* lDocument = InContext.Assets.Find(lPath);
                    if (lDocument == nullptr)
                    {
                        return AutomationResult::Fail("the asset is not open (asset.open)");
                    }

                    if (bInUndo) { InContext.Assets.Undo(lPath); }
                    else         { InContext.Assets.Redo(lPath); }
                    return AutomationResult::Ok(DescribeAsset(InContext, *lDocument));
                });
            };
            lStep("asset.undo", "Undoes the last change to the open asset at {path} (its own history).", true);
            lStep("asset.redo", "Redoes the last undone change to the open asset at {path}.", false);

            InCommands.Register("asset.save",
                "Writes the open asset at {path} and reloads it, so a playing game reads the new values.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    const AssetDocument* lDocument = InContext.Assets.Find(lPath);
                    if (lDocument == nullptr)
                    {
                        return AutomationResult::Fail("the asset is not open (asset.open, asset.set)");
                    }
                    if (!InContext.Assets.Save(lPath))
                    {
                        return AutomationResult::Fail("the asset was not written (a class no module registered is "
                                                      "read-only; see the log)");
                    }
                    return AutomationResult::Ok(DescribeAsset(InContext, *lDocument));
                });

            InCommands.Register("asset.close",
                "Closes the window of the asset at {path}. Unsaved changes are refused unless \"discard\": true.",
                [&InContext](const nlohmann::json& InParams)
                {
                    OpaaxString lPath;
                    OpaaxString lError;
                    if (!AssetPath(InContext, InParams, lPath, lError))
                    {
                        return AutomationResult::Fail(lError);
                    }

                    if (!InContext.Assets.Close(lPath, InParams.value("discard", false)))
                    {
                        return AutomationResult::Fail("the asset has unsaved changes: save them first (asset.save), or "
                                                      "pass \"discard\": true to lose them");
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "path", InContext.Paths.AbsoluteToAsset(lPath).CStr() } });
                },
                "save the asset first (asset.save), or pass \"discard\": true to lose its changes");

            InCommands.Register("asset.list",
                "The open assets: {assets: [{path, class, unsaved, ...}]}, in the order they were opened.",
                [&InContext](const nlohmann::json&)
                {
                    nlohmann::json lList = nlohmann::json::array();
                    for (const AssetDocument* lDocument : InContext.Assets.GetOpen())
                    {
                        nlohmann::json lEntry = DescribeAsset(InContext, *lDocument);
                        lEntry.erase("properties");
                        lList.push_back(Move(lEntry));
                    }
                    return AutomationResult::Ok(nlohmann::json{ { "assets", lList } });
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
        RegisterUI(lCommands, InContext);
        RegisterAssets(lCommands, InContext);
    }
}
