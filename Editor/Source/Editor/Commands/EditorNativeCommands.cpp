#include "Editor/Commands/EditorNativeCommands.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorGizmo.hpp"   // SetMode — the three gizmo commands (③)
#include "Editor/Operation/EditorSelection.hpp"   // the primary — what the Inspector's verbs act on
#include "Editor/Undo/EditorUndo.h"           // the two commands that drive the stack (⑤)
#include "Editor/Operation/EntityOps.h"
#include "Editor/Operation/LevelOperations.h"
#include "Editor/Operation/SheetOperations.h"
#include "Editor/Operation/ClipOperations.h"
#include "Editor/Operation/LibraryOperations.h"
#include "Editor/Operation/FontFamilyOperations.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationLibraryDocument.h"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"
#include "Editor/Panels/EditorPanels.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/Platforms/IFileSystem.h"
#include "Core/Window/Window.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Entity/Entity.h"   // EntityOps::Create returns one by value
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"

#include "Editor/UI/IEditorDialogs.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorCommands{"EditorCommands"};

    using namespace Opaax::Editor;

    /**
     * A map that belongs to no open level: a fresh world with an empty Level holding only it,
     * rather than merging it into a level it is not part of.
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

            // No manifest behind this world — an empty level path is what tells the document so.
            LevelOps::AdoptOpen(InContext, OpaaxString());
        });
    }

    /**
     * A file-picker request in the editor's one shape.
     *
     * Every caller below states a title, a starting path and one extension, so the shape is stated
     * here once instead of five times.
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
     * The whole of New Map AFTER a destination has been chosen.
     *
     * Split out of the command rather than nested in its lambda so the interesting half stays
     * readable — and callable with no dialog in front of it.
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

        // NEW MEANS NEW. The OS save dialog warns about overwriting, but "New Map" truncating a map
        // that already has entities in it is not a thing to leave to a dialog the author is used to
        // clicking through. Open Map and Add Map are the verbs for a file that exists.
        if (InContext.FileSystem.IsPathExist(InAbsPath))
        {
            OPAAX_LOG(LogEditorCommands, Warn,
                      "'{}' already exists — use Open Map or Level/Add Map... instead of overwriting it",
                      lAssetRel.CStr());
            return;
        }

        // WRITTEN BEFORE IT IS MOUNTED, because AddMap loads it through the ResourceManager and
        // there has to be a file to load. Stamped with its own id (**MP10**) rather than left
        // anonymous: that is what makes a map with nothing in it an ORDINARY map from its first
        // frame — saveable, removable, and settable as persistent like any other.
        MapData lData;
        lData.Id = MapFile::StemId(InAbsPath);

        if (!MapFile::Save(InAbsPath, lData))
        {
            return; // MapFile logged which of the reasons it was
        }

        if (!InLevel.AddMap(lAssetRel))
        {
            return; // Level logged it — already in this level, or it would not mount
        }

        // RECONCILE, never re-adopt (**MP5**): the new map gets a record, every other map keeps the
        // baseline it had.
        InContext.LevelDocument.TrackMounted(InLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        // BOTH HALVES LAND TOGETHER. Writing the map file and leaving the membership pending was
        // the worst of both: close the editor and the file stayed while the level forgot it.
        InContext.LevelDocument.SaveManifest(InLevel);

        // Focused, because the only reason to make a map is to start putting things in it.
        MapOps::Focus(InContext, lAssetRel);

        OPAAX_LOG(LogEditorCommands, Info, "Created '{}' in level '{}'",
                  lAssetRel.CStr(), InLevel.GetData().Name.CStr());
    }

    /** The whole of Add Map to Level after a file has been chosen. */
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

        // Mounts immediately: every map of the level is in the world (WM1a), so one that was just
        // added is no exception.
        if (InLevel.AddMap(lAssetRel))
        {
            // RECONCILE, never re-adopt: a fresh AdoptExisting would re-take every baseline from
            // the world and quietly declare every other map's unsaved edits to be the clean state.
            InContext.LevelDocument.TrackMounted(InLevel, *InContext.Worlds.GetActiveWorld(),
                                                 InContext.Engine.GetRegistries().Components(),
                                                 InContext.Paths);

            // Structure goes to disk as it changes (EditorLevelDocument::SaveManifest).
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
        // No log here on purpose: EditorPanels::SetVisible is the single mutation point and announces
        // it, so the close button — which cannot reach a command — is heard on the same line.
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

        // WHICH map is this? Asked of the FILE's entities (WM2). The identity decides whether it
        // is already in the world; the path could not, arriving in one shape from a file dialog
        // and another from AssetToAbsolute.
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
            // ALREADY IN THE WORLD, so this loads nothing: every map of the open level is mounted
            // (WM1a). It moves the CURSOR — the selection survives untouched because none of the
            // entities it points at go anywhere, and NOTHING IS CONFIRMED because nothing is at
            // risk: every map keeps its own baseline (MP5), so the map being left stays as dirty
            // as it was and Save Level will still write it.
            InContext.MapDocument.Focus(InParams.AbsPath);
            return;
        }

        OpenStandaloneMap(InContext, InParams.AbsPath);
    }

    // =========================================================================
    // Entity — thin by design. Every body is EntityOps', so the Edit menu, the Hierarchy's context
    // menus and the viewport's keys cannot drift into three behaviours.
    // =========================================================================
    // THE POLICY GATE LIVES HERE, not in the stack: rewinding authored state while a Play clone is
    // active would be written over by the next Stop, and EditorUndo deliberately knows nothing
    // about worlds (⑤).
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
        // The TARGET RIDES IN THE PAYLOAD, defaulting to the focused map — which is all a menu entry
        // can name. The Hierarchy's header menu says WHICH map because it was clicked, and it used to
        // reach EntityOps directly for exactly that reason; carrying the id is what let it come back
        // through the dispatch, and therefore be recorded (⑤).
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
            SaveMapAsCommand{}.Execute(InContext, NoParams{}); // nothing to overwrite — ask where
            return;
        }

        // ONE map — the FOCUSED one, because this entry is on the File menu and the cursor is what
        // a File command has. The Hierarchy's per-map Save names its target instead (MapOps).
        MapOps::Save(InContext, InContext.MapDocument.GetMapId());
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
                    // The cursor follows the file it just wrote; the record already moved with it.
                    InContext.MapDocument.Focus(InPicked);
                }
            });
    }

    // =============================================================================
    // Level — the manifest is edited THROUGH the world's Level (WM1a), never through a second
    // copy here: the Level is what mounts and unmounts, so it is what knows the truth.
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

        SheetOps::Save(InContext);   // SheetOps logs the write and rebases the dirty marker
    }

    void SaveClipCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.ClipDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Clip ignored — no animation clip is open");
            return;
        }

        ClipOps::Save(InContext);   // ClipOps logs the write, rebases the marker and publishes it
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

    void SaveFamilyCommand::Execute(EditorContext& InContext, const Params&)
    {
        if (!InContext.FamilyDocument.IsOpen())
        {
            OPAAX_LOG(LogEditorCommands, Warn, "Save Family ignored — no font family is open");
            return;
        }

        FamilyOps::Save(InContext);
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
