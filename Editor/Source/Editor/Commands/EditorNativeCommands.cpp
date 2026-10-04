#include "Editor/Commands/EditorNativeCommands.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorGizmo.hpp"   // SetMode
#include "Editor/Operation/EditorSelection.hpp"   // the primary selection
#include "Editor/Undo/EditorUndo.h"           // Undo / Redo
#include "Editor/Undo/EntityUndoables.h"      // EntityDelete
#include "World/Serialization/MapSerializer.h"   // CaptureEntities
#include "Editor/Operation/EntityOps.h"
#include "Editor/Operation/LevelOperations.h"
#include "Editor/Operation/SheetOperations.h"
#include "Editor/Operation/ClipOperations.h"
#include "Editor/Operation/LibraryOperations.h"
#include "Editor/Operation/InputOperations.h"
#include "Editor/Operation/DataAssetOperations.h"
#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"
#include "Editor/Operation/MoverOperations.h"
#include "Editor/Operation/FontFamilyOperations.h"
#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/EditorUICanvasDocument.h"
#include "Editor/Panels/UICanvasPanel.h"
#include "Application/OpaaxApplication.h"
#include "Application/Services/IProjectManager.h"   // new UI uses the project's reference height
#include "Engine/Registries/EngineRegistries.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIPanel.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationLibraryDocument.h"
#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"
#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Panels/PrefabPanel.h"
#include "Renderer/RenderTarget.hpp"   // PrefabPanel holds one by TUniquePtr (its dtor needs the type)
#include "Editor/Prefab/EditorPrefabDocument.h"

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Application/Services/IPaths.h"
#include "Platform/IFileSystem.h"
#include "Window/Window.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Entity/Entity.h"   // EntityOps::Create returns one
#include "World/Entity/EntityHierarchy.h"   // Delete takes the subtree
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"

#include "Editor/UI/IEditorDialogs.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogEditorCommands{"EditorCommands"};

    using namespace Opaax::Editor;

    /**
     * A map that belongs to no open level: a new world with an empty Level holding only this map.
     */
    void OpenStandaloneMap(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' is outside the project's Assets — a map has to be an asset of this project to be opened",
                      InAbsPath.CStr());
            return;
        }

        LevelOps::ConfirmDiscardingEdits(InContext, [&InContext, lAssetRel]
        {
            World* const lWorld = InContext.Engine.OpenLevel(WorldSpec{OpaaxString(), EWorldMode::Edit});
            if (lWorld == nullptr || lWorld->GetLevel() == nullptr)
            {
                OPAAX_LOG(LogEditorCommands, Error, "Could not open a world for '{}'", lAssetRel.CStr());
                return;
            }

            lWorld->GetLevel()->Mount(lAssetRel);

            // No level file behind this world: an empty level path says so.
            LevelOps::AdoptOpen(InContext, OpaaxString());
        });
    }

    /**
     * A file-picker request: title, starting path, one extension.
     */
    FileDialogRequest MakeFileRequest(const char* InTitle, OpaaxString InDefaultPath,
                                      const char* InPattern, const char* InDescription)
    {
        FileDialogRequest lRequest;
        lRequest.Title             = OpaaxString(InTitle);
        lRequest.DefaultPath       = Move(InDefaultPath);
        lRequest.FilterDescription = OpaaxString(InDescription);
        lRequest.Filters.emplace_back(InPattern);

        return lRequest;
    }

    /**
     * New Map, after the destination was chosen.
     */
    void CreateMapAt(EditorContext& InContext, Level& InLevel, const OpaaxString& InAbsPath)
    {
        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InAbsPath);

        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' is outside the project's Assets — a level can only name assets of this project",
                      InAbsPath.CStr());
            return;
        }

        // New means new: an existing file is refused (Open Map and Add Map are for those).
        if (InContext.FileSystem.IsPathExist(InAbsPath))
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' already exists — use Open Map or Level/Add Map... instead of overwriting it",
                      lAssetRel.CStr());
            return;
        }

        // Written before it is loaded (AddMap loads it from disk), with its own map id, so an empty map is
        // an ordinary map: saveable, removable, can be made persistent.
        MapData lData;
        lData.Id = MapFile::StemId(InAbsPath);

        if (!MapFile::Save(InAbsPath, lData))
        {
            return; // MapFile logged why
        }

        if (!InLevel.AddMap(lAssetRel))
        {
            return; // Level logged why
        }

        // Reconcile, not re-adopt: the new map gets a record, every other map keeps its baseline.
        InContext.LevelDocument.TrackMounted(InLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        // The map file and the level change land together.
        InContext.LevelDocument.SaveManifest(InLevel);

        // Focused: a new map is made to be filled.
        MapOps::Focus(InContext, lAssetRel);

        OPAAX_LOG(LogEditorCommands, Info, "Created '{}' in level '{}'",
                  lAssetRel.CStr(), InLevel.GetData().Name.CStr());
    }

    /** Add Map to Level, after a file was chosen. */
    void AddMapAt(EditorContext& InContext, Level& InLevel, const OpaaxString& InAbsPath)
    {
        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InAbsPath);

        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' is outside the project's Assets — a manifest can only name assets of this project",
                      InAbsPath.CStr());
            return;
        }

        // Loaded immediately: every map of the level is in the world.
        if (InLevel.AddMap(lAssetRel))
        {
            // Reconcile, not re-adopt: re-adopting would reset every other map's baseline and hide its
            // unsaved edits.
            InContext.LevelDocument.TrackMounted(InLevel, *InContext.Worlds.GetActiveWorld(),
                                                 InContext.Engine.GetRegistries().Components(),
                                                 InContext.Paths);

            // Level structure is saved immediately (EditorLevelDocument::SaveManifest).
            InContext.LevelDocument.SaveManifest(InLevel);
        }
    }
}

namespace Opaax::Editor
{
    // =============================================================================
    // App
    // =============================================================================

    void QuitCommand::Execute(EditorContext& InContext, const Params&)
    {
        OPAAX_LOG(LogEditorCommands, Info, "Exit requested");

        InContext.MainWindow.RequestClose();
    }

    void MinimizeWindowCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.MainWindow.Minimize();
    }

    void ToggleMaximizeWindowCommand::Execute(EditorContext& InContext, const Params&)
    {
        Window& lWindow = InContext.MainWindow;

        if (lWindow.IsMaximized()) { lWindow.Restore(); }
        else                       { lWindow.Maximize(); }
    }

    void TogglePanelCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        // Not logged here: EditorPanels::SetVisible logs it (also for the window's close button).
        InContext.Panels.SetVisible(InParams.PanelId, !InContext.Panels.IsVisible(InParams.PanelId));
    }

    // =============================================================================
    // Play in editor
    // =============================================================================

    void PlayCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PIE.Play();
    }

    void TogglePauseCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PIE.TogglePause();
    }

    void StepCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PIE.Step();
    }

    void StopCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PIE.Stop();
    }

    // =============================================================================
    // Map
    // =============================================================================

    void NewMapCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "New Map")) { return; }

        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        InContext.Dialogs.SaveFile(
            MakeFileRequest("New Map", InContext.Paths.AssetToAbsolute(OpaaxString("Maps/NewMap.opaaxmap")),
                            "*.opaaxmap", "Opaax Map"),
            [&InContext, lLevel](const OpaaxString& InPicked)
            {
                CreateMapAt(InContext, *lLevel, InPicked);
            });
    }

    void OpenMapCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Open Map")) { return; }

        InContext.Dialogs.OpenFile(
            MakeFileRequest("Open Map", InContext.Paths.AssetToAbsolute(OpaaxString("Maps/")),
                            "*.opaaxmap", "Opaax Map"),
            [&InContext](const OpaaxString& InPicked)
            {
                OpenMapAtCommand{}.Execute(InContext, MapPathParams{InPicked});
            });
    }

    void OpenMapAtCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        if (!MapOps::CanEdit(InContext, "Open Map")) { return; }

        // Which map is it? Read from the file's entities (paths come in different forms).
        MapData lData;
        if (!MapFile::Load(InParams.AbsPath, lData))
        {
            OPAAX_LOG(LogEditorCommands, Error, "Open Map FAILED for '{}' — nothing changed",
                      InParams.AbsPath.CStr());
            return;
        }

        const Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lLevel != nullptr && lLevel->IsMounted(lData.Id))
        {
            // Already in the world: nothing loads, only the cursor moves. The selection is kept, and nothing
            // is at risk (every map keeps its own baseline), so no confirmation.
            InContext.MapDocument.Focus(InParams.AbsPath);
            return;
        }

        OpenStandaloneMap(InContext, InParams.AbsPath);
    }

    // =========================================================================
    // Entity — thin wrappers over EntityOps, so the Edit menu, the Hierarchy menus and the viewport keys
    // behave the same.
    // The Play-mode check lives here, not in the undo stack (it knows nothing about worlds).
    void UndoCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Undo")) { return; }

        InContext.Undo.Undo(InContext);
    }

    void RedoCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Redo")) { return; }

        InContext.Undo.Redo(InContext);
    }

    void CreateEntityCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        // The target map is in the payload; invalid means the focused map (all a menu entry can name).
        // The Hierarchy's header menu passes the clicked map.
        const MapId lMap = InParams.Map.IsValid() ? InParams.Map : InContext.MapDocument.GetMapId();

        EntityOps::Create(InContext, lMap, OpaaxString("Entity"));
    }

    void DeleteSelectedCommand::Execute(EditorContext& InContext, const Params&)
    {
        EntityOps::DestroySelected(InContext);
    }

    void RenameSelectedCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        EntityOps::Rename(InContext, InContext.Selection.Get(), InParams.Name);
    }

    void AddComponentCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        EntityOps::AddComponent(InContext, InContext.Selection.Get(), InParams.TypeName);
    }

    void RemoveComponentCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        EntityOps::RemoveComponent(InContext, InContext.Selection.Get(), InParams.TypeName);
    }

    void TransformSelectedCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        EntityOps::TransformSelected(InContext, InParams);
    }

    void FocusSelectedCommand::Execute(EditorContext& InContext, const Params&)
    {
        EntityOps::FocusSelected(InContext);
    }

    void GizmoTranslateCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.Gizmo.SetMode(EGizmoMode::Translate);
    }

    void GizmoRotateCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.Gizmo.SetMode(EGizmoMode::Rotate);
    }

    void GizmoScaleCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.Gizmo.SetMode(EGizmoMode::Scale);
    }

    void SaveMapCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Save Map")) { return; }

        if (!InContext.MapDocument.HasMap())
        {
            SaveMapAsCommand{}.Execute(InContext, NoParams{}); // no file yet: ask where
            return;
        }

        // The focused map (a File menu command). The Hierarchy's per-map Save names its target.
        MapOps::Save(InContext, InContext.MapDocument.GetMapId());
    }

    void InstantiatePrefabAtCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        // The focused map, as for SaveMapCommand. EntityOps refuses (with a warning) when there is none.
        EntityOps::InstantiatePrefab(InContext, InParams.AbsPath, InContext.MapDocument.GetMapId());
    }

    void CreatePrefabFromSelectionCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Create Prefab")) { return; }

        if (InContext.Selection.Count() == 0)
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Create Prefab — nothing is selected.");
            return;
        }

        // Default name: the primary entity's name. Only a suggestion the dialog can change.
        OpaaxString lSuggested = OpaaxString("Prefab");
        if (World* const lWorld = InContext.Worlds.GetActiveWorld(); lWorld != nullptr)
        {
            Entity lPrimary{ InContext.Selection.Ids().front(), lWorld };
            if (lPrimary.IsValid()) { lSuggested = lPrimary.Get<EntityMeta>().Name; }
        }

        InContext.Dialogs.SaveFile(
            MakeFileRequest("Create Prefab",
                            InContext.Paths.AssetToAbsolute(
                                OpaaxString("Prefabs/") + lSuggested + OpaaxString(".opaaxprefab")),
                            "*.opaaxprefab", "Opaax Prefab"),
            [&InContext](const OpaaxString& InPicked)
            {
                EntityOps::CreatePrefabFromSelection(InContext, InPicked);
            });
    }

    void RevertToPrefabCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        EntityOps::RevertToPrefab(InContext, InParams.bWholeInstance);
    }

    void OpenPrefabAtCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        if (InContext.PrefabDocument.Open(InContext, InParams.AbsPath))
        {
            InContext.Panels.SetVisible(PrefabPanel::PanelID(), true);
        }
    }

    void SavePrefabCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PrefabDocument.Save(InContext);
    }

    void SavePrefabAsVariantCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.PrefabDocument.IsOpen()) { return; }

        // "<Base>Variant.opaaxprefab", beside the base.
        const OpaaxString& lBase      = InContext.PrefabDocument.AbsPath();
        const Int32        lExtension = lBase.Find(".opaaxprefab");
        const OpaaxString  lSuggested = (lExtension >= 0 ? lBase.SubString(0, static_cast<Uint32>(lExtension)) : lBase)
                                        + OpaaxString("Variant.opaaxprefab");

        InContext.Dialogs.SaveFile(
            MakeFileRequest("Save As Variant", lSuggested, "*.opaaxprefab", "Opaax Prefab"),
            [&InContext](const OpaaxString& InPicked)
            {
                InContext.PrefabDocument.SaveAsVariant(InContext, InPicked);
            });
    }

    void UndoPrefabCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PrefabDocument.Undo().Undo(InContext);
    }

    void RedoPrefabCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.PrefabDocument.Undo().Redo(InContext);
    }

    void DeletePrefabSelectionCommand::Execute(EditorContext& InContext, const Params&)
    {
        EditorSelection& lSelection = InContext.PrefabDocument.Selection();
        World* const     lWorld     = InContext.PrefabDocument.GetWorld();

        if (!lSelection.HasSelection() || lWorld == nullptr || lSelection.GetWorld() != lWorld)
        {
            return;   // nothing selected (normal)
        }

        // Copy the handles first (the selection is about to be cleared), with the subtree (undo must
        // restore all of it).
        TDynArray<EntityID> lIds;
        EntityHierarchy::CollectSubtree(*lWorld, lSelection.Ids(), lIds);

        // Captured before destroying.
        EntityDelete lStep{ MapSerializer::CaptureEntities(
            *lWorld, InContext.Engine.GetRegistries().Components(), lIds), EUndoWorld::Prefab };

        lSelection.Clear();

        const Uint64 lCount = EntityOps::DestroyEntities(*lWorld, lIds);

        InContext.PrefabDocument.Undo().Record(Move(lStep));

        OPAAX_LOG(LogEditorCommands, Info, "Deleted {} entity(ies) from the prefab", lCount);
    }

    void SaveMapAsCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Save Map As")) { return; }

        InContext.Dialogs.SaveFile(
            MakeFileRequest("Save Map As",
                            InContext.MapDocument.HasMap()
                                ? InContext.MapDocument.AbsPath()
                                : InContext.Paths.AssetToAbsolute(OpaaxString("Maps/Untitled.opaaxmap")),
                            "*.opaaxmap", "Opaax Map"),
            [&InContext](const OpaaxString& InPicked)
            {
                if (InContext.LevelDocument.SaveMapAs(InContext.MapDocument.GetMapId(), InPicked,
                                                      *InContext.Worlds.GetActiveWorld(),
                                                      InContext.Engine.GetRegistries().Components()))
                {
                    // The cursor follows the file just written; the record already moved with it.
                    InContext.MapDocument.Focus(InPicked);
                }
            });
    }

    // =============================================================================
    // Level — the level file is edited through the world's Level (it loads and unloads the maps).
    // =============================================================================

    void OpenLevelCommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.Dialogs.OpenFile(
            MakeFileRequest("Open Level", InContext.Paths.AssetToAbsolute(OpaaxString("Levels/")),
                            "*.opaaxlevel", "Opaax Level"),
            [&InContext](const OpaaxString& InPicked)
            {
                OpenLevelAtCommand{}.Execute(InContext, LevelPathParams{InPicked});
            });
    }

    void OpenLevelAtCommand::Execute(EditorContext& InContext, const Params& InParams)
    {
        if (!MapOps::CanEdit(InContext, "Open Level")) { return; }

        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InParams.AbsPath);
        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' is outside the project's Assets — a level has to be an asset of this project",
                      InParams.AbsPath.CStr());
            return;
        }

        LevelOps::ConfirmDiscardingEdits(InContext, [&InContext, lAssetRel, InParams]
        {
            if (InContext.Engine.OpenLevel(WorldSpec{lAssetRel, EWorldMode::Edit}) == nullptr)
            {
                OPAAX_LOG(LogEditorCommands, Error, "Could not open level '{}'", lAssetRel.CStr());
                return;
            }

            LevelOps::AdoptOpen(InContext, InParams.AbsPath);
        });
    }

    void SaveSheetCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.SheetDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Sheet ignored — no sprite sheet is open");
            return;
        }

        SheetOps::Save(InContext);   // logs the write, updates the dirty baseline
    }

    void SaveClipCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.ClipDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Clip ignored — no animation clip is open");
            return;
        }

        ClipOps::Save(InContext);   // logs the write, updates the baseline, reloads
    }

    void SaveLibraryCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.LibraryDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Library ignored — no animation library is open");
            return;
        }

        LibraryOps::Save(InContext);
    }

    void SaveMoveModeCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.MoveModeDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Move Mode ignored — no move mode is open");
            return;
        }

        MoveModeOps::Save(InContext);
    }

    void SaveDataAssetCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.DataAssetDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Data Asset ignored — no data asset is open");
            return;
        }

        DataAssetOps::Save(InContext);
    }

    void SaveMoverCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.MoverDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Mover ignored — no mover is open");
            return;
        }

        MoverOps::Save(InContext);
    }

    void SaveInputActionCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.InputActionDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Input Action ignored — no input action is open");
            return;
        }

        InputActionOps::Save(InContext);
    }

    void SaveInputMapCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.InputMapDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Input Map ignored — no input mapping context is open");
            return;
        }

        InputMapOps::Save(InContext);
    }

    void SaveFamilyCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.FamilyDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Family ignored — no font family is open");
            return;
        }

        FamilyOps::Save(InContext);
    }

    // =============================================================================
    // UI canvas
    // =============================================================================

    void NewUICommand::Execute(EditorContext& InContext, const Params&)
    {
        InContext.Dialogs.SaveFile(
            MakeFileRequest("New UI", InContext.Paths.AssetToAbsolute(OpaaxString("UI/NewUI.opaaxui")),
                            "*.opaaxui", "Opaax UI Canvas"),
            [&InContext](const OpaaxString& InPicked)
            {
                // New means new (as NewMapCommand): an existing canvas is not overwritten.
                if (InContext.FileSystem.IsPathExist(InPicked))
                {
                    OPAAX_LOG(LogEditorCommands, Warn,
                              "'{}' already exists — open it from the Resource Browser instead of overwriting it",
                              InPicked.CStr());
                    return;
                }

                // Authored at the project's reference height, so the panel shows what the game draws.
                UICanvasFile::UICanvasDoc lDoc;
                lDoc.ReferenceHeight = OpaaxApplication::GetAppService<IProjectManager>().UIReferenceHeight();
                lDoc.Root            = MakeUnique<UIPanel>();
                lDoc.Root->Name      = "Root";

                if (!UICanvasFile::Save(InPicked, lDoc))
                {
                    return;   // UICanvasFile logged why
                }

                const UIWidgetRegistry& lRegistry =
                    OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets();

                if (InContext.UICanvasDocument.Open(InPicked, lRegistry))
                {
                    InContext.Panels.SetVisible(UICanvasPanel::PanelID(), true);
                }
            });
    }

    void SaveUICommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.UICanvasDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save UI ignored — no UI canvas is open");
            return;
        }

        UICanvasOps::Save(InContext);
    }

    void DeleteUIWidgetCommand::Execute(EditorContext& InContext, const Params&)
    {
        const UIWidgetPath& lSelected = InContext.UICanvasDocument.SelectedPath();
        if (lSelected.empty()) { return; }   // nothing selected (normal)

        UICanvasOps::RemoveWidget(InContext, lSelected);
    }

    void SaveLevelCommand::Execute(EditorContext& InContext, const Params&)
    {
        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr || !InContext.LevelDocument.HasLevel())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Level ignored — no level file is open");
            return;
        }

        InContext.LevelDocument.SaveAll(*InContext.Worlds.GetActiveWorld(),
                                        InContext.Engine.GetRegistries().Components(), *lLevel);
    }

    void AddMapToLevelCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!MapOps::CanEdit(InContext, "Add Map")) { return; }

        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        InContext.Dialogs.OpenFile(
            MakeFileRequest("Add Map to Level", InContext.Paths.AssetToAbsolute(OpaaxString("Maps/")),
                            "*.opaaxmap", "Opaax Map"),
            [&InContext, lLevel](const OpaaxString& InPicked)
            {
                AddMapAt(InContext, *lLevel, InPicked);
            });
    }
}
