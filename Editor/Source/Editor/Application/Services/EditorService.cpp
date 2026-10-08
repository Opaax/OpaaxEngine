#include "Editor/Application/Services/EditorService.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Editor/Automation/EditorAutomationCommands.h"
#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Application/Services/IProjectManager.h"
#include "Platform/IFileSystem.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IWindowManager.h"
#include "Core/Events/Event.h"
#include "Editor//Application/Services/EditorPaths.h"
#include "Editor/Commands/EditorNativeCommands.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Imgui/ImGuiEditorGui.h"        // the concrete IEditorGui
#include "Editor/UI/TinyFdEditorDialogs.h"      // the concrete IEditorDialogs
#include "Editor/Toolbar/EditorNativeViewportTools.h"

#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorViewport.hpp"   // grid toggle
#include "Editor/Operation/LevelOperations.h"
#include "Editor/Panels/CameraPreviewPanel.h"
#include "Editor/Panels/ConfigPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/Panels/ResourcePreviewPanel.h"
#include "Editor/Panels/AnimationClipPanel.h"
#include "Editor/Panels/AnimationLibraryPanel.h"
#include "Editor/Panels/MoveModePanel.h"
#include "Editor/Panels/DataAssetPanel.h"
#include "Resources/DataAsset/DataAssetResource.h"
#include "Editor/Panels/InputActionPanel.h"
#include "Editor/Panels/InputMappingContextPanel.h"
#include "Editor/Panels/MoverPanel.h"
#include "Editor/Panels/FontFamilyPanel.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/Panels/UICanvasPanel.h"
#include "Editor/EditorUICanvasDocument.h"
#include "UI/UICanvasFile.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"
#include "UI/UICanvasResource.h"
#include "Editor/Panels/PrefabPanel.h"
#include "Renderer/RenderTarget.hpp"
#include "Editor/Panels/SpriteSheetPanel.h"
#include "Editor/Panels/LogPanel.h"
#include "Editor/Panels/StatsPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Imgui/Configs/Config_EditorImgui.h"
#include "Editor/Imgui/Configs/EditorImguiConfigDrawer.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Renderer/Config/Config_Renderer.h"
#include "Input/InputEvents.h"
#include "World/Serialization/LevelResource.hpp"
#include "World/Serialization/MapResource.hpp"
#include "World/Prefab/PrefabResource.hpp"
#include "Editor/Operation/ResourceOperations.h"
#include "Resources/ResourceTypeID.hpp"
#include "Renderer/Textures/TextureResource.h"
#include "Renderer/Textures/SpriteSheetResource.h"
#include "Animation/AnimationClipResource.h"
#include "Animation/AnimationLibraryResource.h"
#include "Movement/Assets/MoveModeResource.h"
#include "Movement/Assets/MoverResource.h"
#include "Input/Assets/InputActionResource.h"
#include "Input/Assets/InputMappingContextResource.h"
#include "Renderer/Text/FontFaceResource.h"
#include "Renderer/Text/FontFamilyResource.h"
#include "Editor/Resources/ResourcePreviewDrawers.h"   // what a preview draws (no ImGui here)
#include "Editor/Properties/NativeComponentDrawers.h"
#include "Editor/Properties/WidgetPropertyVisitor.h"
#include "World/Components/ComponentRegistry.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Entity/Entity.h"
#include "Renderer/Camera/CameraComponent.h"      // engine components
#include "Physics/Components/ColliderComponent.h"    // drawn by default
#include "Renderer/Components/QuadComponent.h"
#include "Movement/MoverComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "Physics/Components/RigidbodyComponent.h"
#include "Animation/SpriteAnimatorComponent.h"
#include "Renderer/Components/TextComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogEditorService{"EditorService"};

    using namespace Opaax::Editor;

    /** True while the edit world is on screen (MapOps::CanEdit's rule). */
    bool IsEditing(const EditorContext& InContext) { return InContext.PIE.IsEdit(); }

    /** True while a Play clone is running or paused. */
    bool IsPlaying(const EditorContext& InContext) { return !InContext.PIE.IsEdit(); }

    bool IsPaused(const EditorContext& InContext) { return InContext.PIE.IsPaused(); }
}

namespace Opaax::Editor
{
    // The only place the editor picks a UI backend. Built here so m_Gui is never null (the context
    // references it). The dialog backend is picked separately (OS dialogs, not ImGui).
    EditorService::EditorService()
        : m_Gui(MakeUnique<ImGuiEditorGui>())
        , m_Dialogs(MakeUnique<TinyFdEditorDialogs>())
    {
    }

    // =============================================================================
    // =============================================================================
    // Editor Native
    // =============================================================================
    // =============================================================================

    void EditorService::CacheEditorPaths()
    {
        /* EditorSaveDir/EditorAssetsDir live only on EditorPaths, deliberately: 
        The engine's IPaths knows nothing about an editor. 
        EditorApplication::CreatePaths normally installs EditorPaths, 
        but it falls back to a plain Paths when no edited project is declared — so this cast genuinely can fail. */
        const IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();
        m_EditorPaths = dynamic_cast<const EditorPaths*>(&lPaths);

        if (m_EditorPaths == nullptr)
        {
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths (no edited project?) — editor space unavailable.");
        }
    }

    void EditorService::CreateEditorSystems(IEngine& InEngine)
    {
        m_Selection         = MakeUnique<EditorSelection>();
        m_ResourceEvents    = MakeUnique<EditorResourceEvents>();
        m_PrefabDocument    = MakeUnique<EditorPrefabDocument>();
        m_Viewport          = MakeUnique<EditorViewport>();
        m_Preview           = MakeUnique<ResourcePreview>();
        m_Camera            = MakeUnique<EditorCamera>();
        m_Gizmo             = MakeUnique<EditorGizmo>();
        m_Undo              = MakeUnique<EditorUndo>();
        m_PIE               = MakeUnique<PlayInEditor>(*m_WorldMgr, InEngine);
        m_InputRoute        = MakeUnique<InputRoute>(*m_WorldMgr, InEngine.GetInput(), *m_PIE);
        m_MapDocument       = MakeUnique<EditorMapDocument>();
        m_LevelDocument     = MakeUnique<EditorLevelDocument>();

        // What prefab paths are resolved with when a map is folded for saving.
        m_LevelDocument->BindPrefabSources(OpaaxApplication::GetAppService<IPaths>(),
                                           OpaaxApplication::GetAppService<IEngine>().GetResources());
        m_SheetDocument     = MakeUnique<EditorSpriteSheetDocument>();
        m_ClipDocument      = MakeUnique<EditorAnimationClipDocument>();
        m_LibraryDocument   = MakeUnique<EditorAnimationLibraryDocument>();
        m_FamilyDocument    = MakeUnique<EditorFontFamilyDocument>();
        m_UICanvasDocument  = MakeUnique<EditorUICanvasDocument>();
        m_MoveModeDocument  = MakeUnique<EditorMoveModeDocument>();
        m_MoverDocument     = MakeUnique<EditorMoverDocument>();
        m_InputActionDocument = MakeUnique<EditorInputActionDocument>();
        m_InputMapDocument    = MakeUnique<EditorInputMappingContextDocument>();
        m_DataAssetDocument   = MakeUnique<EditorDataAssetDocument>();
        m_Export              = MakeUnique<GameExport>();
    }

    void EditorService::ClearEditorSystems()
    {
        m_Selection.reset();
        m_Viewport.reset();
        m_Preview.reset();
        m_Camera.reset();
        m_Gizmo.reset();
        m_Undo.reset();
        m_InputRoute.reset();
        m_PIE.reset();

        // Waits for an export still running.
        m_Export.reset();
    }

    void EditorService::CreateEditorContext(Window* InWindow, IEngine& InEngine)
    {
        // --- EditorContext: built after the UI backend, so it can reference it ------------------------------
        m_Context = MakeUnique<EditorContext>(EditorContext{
            InEngine,
            InEngine.GetWorldManager(),
            InEngine.GetResources(),
            *m_Gui,
            m_Gui->Backend(),
            *m_Dialogs,
            m_Gui->Widgets(),
            *m_Selection,
            *m_ResourceEvents,
            *m_PrefabDocument,
            *m_Viewport,
            *m_Camera,
            *m_Gizmo,
            *m_Undo,
            *m_PIE,
            *m_InputRoute,
            *m_LevelDocument,
            *m_MapDocument,
            *m_SheetDocument,
            *m_ClipDocument,
            *m_LibraryDocument,
            *m_FamilyDocument,
            *m_UICanvasDocument,
            *m_MoveModeDocument,
            *m_MoverDocument,
            *m_InputActionDocument,
            *m_InputMapDocument,
            *m_DataAssetDocument,
            m_Extensions,
            m_Gui->Panels(),
            *m_Preview,
            OpaaxApplication::GetAppService<IPaths>(),
            OpaaxApplication::GetAppService<IPlatform>().GetFileSystem(),
            OpaaxApplication::GetAppService<IConfigSystem>(),
            *InWindow,
            m_EditorPaths,
            *m_Export
        });

        // After the context, because it holds one. The reconciler handles prefab instances (entities,
        // which a plain Reload cannot update).
        m_PrefabReconciler = MakeUnique<PrefabReconciler>(*m_Context);
        m_PrefabReconciler->Bind(*m_ResourceEvents);
    }

    void EditorService::ClearEditorContext()
    {
        // Unbind before the context it holds is destroyed.
        if (m_PrefabReconciler != nullptr && m_ResourceEvents != nullptr)
        {
            m_PrefabReconciler->Unbind(*m_ResourceEvents);
        }

        m_PrefabReconciler.reset();
        m_Context.reset();
    }

    void EditorService::PostInitialized()
    {
        BuildGUIs();
        AdoptStartupLevel();
    }
    
    void EditorService::DrawGUI()
    {
        if (m_Context == nullptr) { return; }







        // Before the UI pass: a shortcut may run a command that destroys the world.
        HandleAuthoringShortcuts();

        // One UI pass, owned by the gui: dockspace, menu bar and every panel (including the Viewport,
        // which samples the FBO Engine().Loop() just rendered).
        m_Gui->Draw(*m_Context);
    }

    // =============================================================================
    // =============================================================================
    // Editor Registers 
    // =============================================================================
    // =============================================================================
    
    void EditorService::RegisterNativeMenus()
    {
        TitleBarRegistry& lMenu = m_Extensions.TitleBar();

        // --- Native File  --------------------------
        EditorTitleBarCategory& lFile = lMenu.Category("File");
        lFile.AddCommand("New Map...", Tags::EDITOR_COMMAND_NEW_MAP).SetEnabled(IsEditing);
        lFile.AddCommand("Open Map...", Tags::EDITOR_COMMAND_OPEN_MAP);
        lFile.AddSeparator();
        lFile.AddCommand("Save Map", Tags::EDITOR_COMMAND_SAVE_MAP).SetEnabled(IsEditing);
        lFile.AddCommand("Save Map As...", Tags::EDITOR_COMMAND_SAVE_MAP_AS).SetEnabled(IsEditing);
        lFile.AddSeparator();
        lFile.AddSeparator();
        lFile.AddCommand("New UI...", Tags::EDITOR_COMMAND_NEW_UI);
        lFile.AddSeparator();
        lFile.AddCommand("Open Level...", Tags::EDITOR_COMMAND_OPEN_LEVEL);
        lFile.AddCommand("Save Level", Tags::EDITOR_COMMAND_SAVE_LEVEL).SetEnabled(IsEditing);
        lFile.AddSeparator();
        lFile.AddCommand("Export Game...", Tags::EDITOR_COMMAND_EXPORT_GAME);
        lFile.AddSeparator();
        lFile.AddCommand("Exit", Tags::EDITOR_COMMAND_QUIT);

        // --- Native Edit  --------------------------
        // Enabled only while editing (they are refused in a Play world anyway).
        EditorTitleBarCategory& lEdit = lMenu.Category("Edit");

        // Undo/Redo first, greyed when empty. The label names the step ("Undo Move (Ctrl+Z)").
        lEdit.AddCommand("Undo", Tags::EDITOR_COMMAND_UNDO)
             .SetEnabled([](const EditorContext& InContext) { return InContext.Undo.CanUndo(); })
             .SetLabel([](const EditorContext& InContext)
             {
                 return InContext.Undo.CanUndo()
                            ? OpaaxString("Undo ") + InContext.Undo.UndoLabel() + OpaaxString(" (Ctrl+Z)")
                            : OpaaxString("Undo (Ctrl+Z)");
             });

        lEdit.AddCommand("Redo", Tags::EDITOR_COMMAND_REDO)
             .SetEnabled([](const EditorContext& InContext) { return InContext.Undo.CanRedo(); })
             .SetLabel([](const EditorContext& InContext)
             {
                 return InContext.Undo.CanRedo()
                            ? OpaaxString("Redo ") + InContext.Undo.RedoLabel() + OpaaxString(" (Ctrl+Y)")
                            : OpaaxString("Redo (Ctrl+Y)");
             });
        lEdit.AddSeparator();

        // An empty map id = the focused map.
        lEdit.AddCommand("Create Entity", Tags::EDITOR_COMMAND_CREATE_ENTITY)
             .SetEnabled(IsEditing)
             .SetParams(MapIdParams{});
        lEdit.AddCommand("Delete Selected", Tags::EDITOR_COMMAND_DELETE_ENTITY).SetEnabled(IsEditing);
        lEdit.AddSeparator();
        lEdit.AddCommand("Focus Selected", Tags::EDITOR_COMMAND_FOCUS_SELECTED).SetEnabled(IsEditing);

        // --- Gizmo mode, as radio entries -------------------------------------------------------------
        // The tick reads the live mode, so the menu and the W/E/R keys always agree.
        lEdit.AddSeparator();
        lEdit.AddCommand("Gizmo: Translate (W)", Tags::EDITOR_COMMAND_GIZMO_TRANSLATE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Translate; });
        lEdit.AddCommand("Gizmo: Rotate (E)", Tags::EDITOR_COMMAND_GIZMO_ROTATE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Rotate; });
        lEdit.AddCommand("Gizmo: Scale (R)", Tags::EDITOR_COMMAND_GIZMO_SCALE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Scale; });

        // --- Native Level  --------------------------
        EditorTitleBarCategory& lLevel = lMenu.Category("Level");
        lLevel.AddCommand("Add Map...", Tags::EDITOR_COMMAND_ADD_MAP_TO_LEVEL).SetEnabled(IsEditing);

        // --- Native Play  --------------------------
        EditorTitleBarCategory& lPlay = lMenu.Category("Play");
        lPlay.AddCommand("Play", Tags::EDITOR_COMMAND_PLAY).SetEnabled(IsEditing);
        lPlay.AddCommand("Pause", Tags::EDITOR_COMMAND_TOGGLE_PAUSE).SetChecked(IsPaused).SetEnabled(IsPlaying);
        lPlay.AddCommand("Step", Tags::EDITOR_COMMAND_STEP).SetEnabled(IsPaused);
        lPlay.AddSeparator();
        lPlay.AddCommand("Stop", Tags::EDITOR_COMMAND_STOP).SetEnabled(IsPlaying);
    }
    
    void EditorService::RegisterNativePanels()
    {
        PanelRegistry& lPanelsRegistry = m_Extensions.Panels();
        
        // Visible by default
        lPanelsRegistry.Register<ViewportPanel>(PanelDesc       {.Id = ViewportPanel::PanelID()});
        lPanelsRegistry.Register<PlayToolbarPanel>(PanelDesc    {.Id = PlayToolbarPanel::PanelID()});
        lPanelsRegistry.Register<HierarchyPanel>(PanelDesc      {.Id = HierarchyPanel::PanelID()});
        lPanelsRegistry.Register<InspectorPanel>(PanelDesc      {.Id = InspectorPanel::PanelID()});
        lPanelsRegistry.Register<ResourceBrowserPanel>(PanelDesc{.Id = ResourceBrowserPanel::PanelID()});
        lPanelsRegistry.Register<LogPanel>(PanelDesc            {.Id = LogPanel::PanelID()});
        
        // Hidden by default
        lPanelsRegistry.Register<CameraPreviewPanel>(PanelDesc  {.Id = CameraPreviewPanel::PanelID(),   .DefaultVisibility = EPanelVisibility::Hidden});
        lPanelsRegistry.Register<ResourcePreviewPanel>(PanelDesc{.Id = ResourcePreviewPanel::PanelID(), .DefaultVisibility = EPanelVisibility::Hidden});
        lPanelsRegistry.Register<SpriteSheetPanel>(PanelDesc    {.Id = SpriteSheetPanel::PanelID(),     .DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_SHEET});
        // Hidden until a prefab is opened. Its SaveCommand routes Ctrl+S to the prefab.
        lPanelsRegistry.Register<PrefabPanel>(PanelDesc         {.Id = PrefabPanel::PanelID(),          .DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_PREFAB, .UndoCommand = Tags::EDITOR_COMMAND_UNDO_PREFAB, .RedoCommand = Tags::EDITOR_COMMAND_REDO_PREFAB, .DeleteCommand = Tags::EDITOR_COMMAND_DELETE_PREFAB_SELECTION});
        lPanelsRegistry.Register<AnimationClipPanel>(PanelDesc  {.Id = AnimationClipPanel::PanelID(),   .DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_CLIP});
        lPanelsRegistry.Register<AnimationLibraryPanel>(PanelDesc{.Id = AnimationLibraryPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_LIBRARY});
        lPanelsRegistry.Register<MoveModePanel>(PanelDesc{.Id = MoveModePanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_MOVE_MODE});
        lPanelsRegistry.Register<MoverPanel>(PanelDesc{.Id = MoverPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_MOVER});
        lPanelsRegistry.Register<DataAssetPanel>(PanelDesc{.Id = DataAssetPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_DATA_ASSET});
        lPanelsRegistry.Register<InputActionPanel>(PanelDesc{.Id = InputActionPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_INPUT_ACTION});
        lPanelsRegistry.Register<InputMappingContextPanel>(PanelDesc{.Id = InputMappingContextPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_INPUT_MAP});
        lPanelsRegistry.Register<FontFamilyPanel>(PanelDesc{.Id = FontFamilyPanel::PanelID(),.DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_FAMILY});
        lPanelsRegistry.Register<UICanvasPanel>(PanelDesc{.Id = UICanvasPanel::PanelID(), .DefaultVisibility = EPanelVisibility::Hidden, .SaveCommand = Tags::EDITOR_COMMAND_SAVE_UI, .DeleteCommand = Tags::EDITOR_COMMAND_DELETE_UI_WIDGET});
        lPanelsRegistry.Register<ConfigPanel>(PanelDesc         {.Id = ConfigPanel::PanelID(),          .DefaultVisibility = EPanelVisibility::Hidden});
        lPanelsRegistry.Register<InputPanel>(PanelDesc          {.Id = InputPanel::PanelID(),           .DefaultVisibility = EPanelVisibility::Hidden });
        lPanelsRegistry.Register<StatsPanel>(PanelDesc          {.Id = StatsPanel::PanelID(),           .DefaultVisibility = EPanelVisibility::Hidden });
    }

    void EditorService::RegisterNativeEditorCommand()
    {
        EditorCommandRegistry& lCommands = m_Extensions.Commands();

        lCommands.Register<QuitCommand>(Tags::EDITOR_COMMAND_QUIT);
        lCommands.Register<MinimizeWindowCommand>(Tags::EDITOR_COMMAND_MINIMIZE_WINDOW);
        lCommands.Register<ToggleMaximizeWindowCommand>(Tags::EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW);
        lCommands.Register<TogglePanelCommand>(Tags::EDITOR_COMMAND_TOGGLE_PANEL);

        lCommands.Register<PlayCommand>(Tags::EDITOR_COMMAND_PLAY);
        lCommands.Register<TogglePauseCommand>(Tags::EDITOR_COMMAND_TOGGLE_PAUSE);
        lCommands.Register<StepCommand>(Tags::EDITOR_COMMAND_STEP);
        lCommands.Register<StopCommand>(Tags::EDITOR_COMMAND_STOP);

        lCommands.Register<CreateEntityCommand>(Tags::EDITOR_COMMAND_CREATE_ENTITY);
        lCommands.Register<DeleteSelectedCommand>(Tags::EDITOR_COMMAND_DELETE_ENTITY);
        lCommands.Register<FocusSelectedCommand>(Tags::EDITOR_COMMAND_FOCUS_SELECTED);

        lCommands.Register<GizmoTranslateCommand>(Tags::EDITOR_COMMAND_GIZMO_TRANSLATE);
        lCommands.Register<GizmoRotateCommand>(Tags::EDITOR_COMMAND_GIZMO_ROTATE);
        lCommands.Register<GizmoScaleCommand>(Tags::EDITOR_COMMAND_GIZMO_SCALE);

        lCommands.Register<NewMapCommand>(Tags::EDITOR_COMMAND_NEW_MAP);
        lCommands.Register<OpenMapCommand>(Tags::EDITOR_COMMAND_OPEN_MAP);
        lCommands.Register<OpenMapAtCommand>(Tags::EDITOR_COMMAND_OPEN_MAP_AT);
        lCommands.Register<InstantiatePrefabAtCommand>(Tags::EDITOR_COMMAND_INSTANTIATE_PREFAB_AT);
        lCommands.Register<CreatePrefabFromSelectionCommand>(Tags::EDITOR_COMMAND_CREATE_PREFAB_FROM_SELECTION);
        lCommands.Register<RevertToPrefabCommand>(Tags::EDITOR_COMMAND_REVERT_TO_PREFAB);
        lCommands.Register<OpenPrefabAtCommand>(Tags::EDITOR_COMMAND_OPEN_PREFAB_AT);
        lCommands.Register<SavePrefabCommand>(Tags::EDITOR_COMMAND_SAVE_PREFAB);
        lCommands.Register<SavePrefabAsVariantCommand>(Tags::EDITOR_COMMAND_SAVE_PREFAB_AS_VARIANT);
        lCommands.Register<UndoPrefabCommand>(Tags::EDITOR_COMMAND_UNDO_PREFAB);
        lCommands.Register<RedoPrefabCommand>(Tags::EDITOR_COMMAND_REDO_PREFAB);
        lCommands.Register<DeletePrefabSelectionCommand>(Tags::EDITOR_COMMAND_DELETE_PREFAB_SELECTION);
        lCommands.Register<SaveMapCommand>(Tags::EDITOR_COMMAND_SAVE_MAP);
        lCommands.Register<SaveMapAsCommand>(Tags::EDITOR_COMMAND_SAVE_MAP_AS);

        lCommands.Register<OpenLevelCommand>(Tags::EDITOR_COMMAND_OPEN_LEVEL);
        lCommands.Register<OpenLevelAtCommand>(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT);
        lCommands.Register<SaveLevelCommand>(Tags::EDITOR_COMMAND_SAVE_LEVEL);
        lCommands.Register<SaveSheetCommand>(Tags::EDITOR_COMMAND_SAVE_SHEET);
        lCommands.Register<SaveClipCommand>(Tags::EDITOR_COMMAND_SAVE_CLIP);
        lCommands.Register<SaveLibraryCommand>(Tags::EDITOR_COMMAND_SAVE_LIBRARY);
        lCommands.Register<SaveMoveModeCommand>(Tags::EDITOR_COMMAND_SAVE_MOVE_MODE);
        lCommands.Register<SaveMoverCommand>(Tags::EDITOR_COMMAND_SAVE_MOVER);
        lCommands.Register<SaveDataAssetCommand>(Tags::EDITOR_COMMAND_SAVE_DATA_ASSET);
        lCommands.Register<SaveInputActionCommand>(Tags::EDITOR_COMMAND_SAVE_INPUT_ACTION);
        lCommands.Register<SaveInputMapCommand>(Tags::EDITOR_COMMAND_SAVE_INPUT_MAP);
        lCommands.Register<SaveFamilyCommand>(Tags::EDITOR_COMMAND_SAVE_FAMILY);

        // A document type gets its own New, like maps and levels.
        lCommands.Register<NewUICommand>(Tags::EDITOR_COMMAND_NEW_UI);
        lCommands.Register<SaveUICommand>(Tags::EDITOR_COMMAND_SAVE_UI);
        lCommands.Register<DeleteUIWidgetCommand>(Tags::EDITOR_COMMAND_DELETE_UI_WIDGET);
        lCommands.Register<AddMapToLevelCommand>(Tags::EDITOR_COMMAND_ADD_MAP_TO_LEVEL);

        lCommands.Register<TransformSelectedCommand>(Tags::EDITOR_COMMAND_TRANSFORM_SELECTED);

        lCommands.Register<RenameSelectedCommand>(Tags::EDITOR_COMMAND_RENAME_SELECTED);
        lCommands.Register<AddComponentCommand>(Tags::EDITOR_COMMAND_ADD_COMPONENT);
        lCommands.Register<RemoveComponentCommand>(Tags::EDITOR_COMMAND_REMOVE_COMPONENT);

        lCommands.Register<UndoCommand>(Tags::EDITOR_COMMAND_UNDO);
        lCommands.Register<RedoCommand>(Tags::EDITOR_COMMAND_REDO);

        lCommands.Register<ExportGameCommand>(Tags::EDITOR_COMMAND_EXPORT_GAME);
    }

    void EditorService::RegisterNativeViewportTools()
    {
        ViewportToolbarRegistry& lTools = m_Extensions.ViewportTools();

        // Order and grouping are decided here; what each tool draws is in
        // Editor/Toolbar/EditorNativeViewportTools.cpp. A game module registers a tool the same way.
        lTools.Add(OPAAX_ID("GizmoMode"), NativeViewportTools::DrawGizmoMode);
        lTools.AddSeparator();

        // Grid next to Snap without a separator: the grid spacing is the translate step.
        lTools.Add(OPAAX_ID("Snap"), NativeViewportTools::DrawSnap);
        lTools.Add(OPAAX_ID("Grid"), NativeViewportTools::DrawGrid);

        // Next to Grid: it changes what the viewport shows, not what a drag does.
        lTools.Add(OPAAX_ID("Colliders"), NativeViewportTools::DrawColliders);
        lTools.AddSeparator();

        lTools.Add(OPAAX_ID("Pivot"), NativeViewportTools::DrawPivot);
        lTools.Add(OPAAX_ID("Space"), NativeViewportTools::DrawSpace);
    }

    void EditorService::RegisterNativeDrawers()
    {
        // The engine's own components, through the same route a game's components use. They are all
        // reflected, so registering is all there is to do.
        // UI widgets too: every widget type needs a drawer or its fields are invisible in the UI panel.
        // The count check below reports a missing one.
        UIWidgetDrawerRegistry& lWidgetDrawers = m_Extensions.UIWidgetDrawers();
        lWidgetDrawers.Register<UIPanel>();
        lWidgetDrawers.Register<UIImage>();
        lWidgetDrawers.Register<UIText>();
        lWidgetDrawers.Register<UIButton>();
        lWidgetDrawers.Register<UIMask>();
        lWidgetDrawers.Register<UISafeArea>();
        lWidgetDrawers.Register<UIStack>();

        // Through the locator, not m_Context: the EditorContext does not exist yet at this point.
        const Uint64 lWidgetTypes =
            OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets().Count();
        if (lWidgetDrawers.Count() != lWidgetTypes)
        {
            OPAAX_LOG(LogEditorService, Warn,
                      "{} UI widget type(s) registered but {} drawer(s) — a type with no drawer shows only "
                      "its base fields (**UI18**)", lWidgetTypes, lWidgetDrawers.Count());
        }
        else
        {
            OPAAX_LOG(LogEditorService, Trace, "UI widget drawers: {} for {} registered type(s)",
                      lWidgetDrawers.Count(), lWidgetTypes);
        }

        ComponentDrawerRegistry& lDrawers = m_Extensions.Drawers();

        // Every entity has one.
        lDrawers.Register<TransformComponent>();

        // Components that name a resource two ways get a custom drawer: it adds one line saying which
        // field the renderer uses, above the normal property list.
        lDrawers.Register<SpriteComponent,          NativeComponentDrawers::SpriteComponentDrawer>();
        lDrawers.Register<SpriteAnimatorComponent,  NativeComponentDrawers::SpriteAnimatorComponentDrawer>();
        lDrawers.Register<TextComponent,            NativeComponentDrawers::TextComponentDrawer>();

        // Takes the context only for its button (opens the Camera Preview); the fields are the normal list.
        lDrawers.Register<CameraComponent,          NativeComponentDrawers::CameraComponentDrawer>();

        // Custom: the component is identity (editing a guid would break the link).
        lDrawers.Register<PrefabInstanceComponent,  NativeComponentDrawers::PrefabInstanceComponentDrawer>();
        lDrawers.Register<QuadComponent>();

        // Reflected, so the generic drawer is enough (enums become dropdowns).
        lDrawers.Register<ColliderComponent>();
        lDrawers.Register<RigidbodyComponent>();
        lDrawers.Register<MoverComponent>();
    }

    void EditorService::RegisterGenericComponentDrawers()
    {
        const ComponentRegistry& lComponents = OpaaxApplication::GetAppService<IEngine>().GetRegistries().Components();
        ComponentDrawerRegistry& lDrawers    = m_Extensions.Drawers();

        lComponents.ForEach([&](const IComponentEntry& InEntry)
        {
            if (!InEntry.IsReflected() || lDrawers.HasTarget(InEntry.GetTypeId()))
            {
                return;
            }

            // The registry owns its entries for the whole run, so the pointer outlives the drawer.
            const IComponentEntry* lEntry = &InEntry;

            lDrawers.RegisterErased(InEntry.GetTypeId(),
                [lEntry](Entity& InSubject, IEditorWidgets& InWidgets, EditorContext&) -> bool
                {
                    EntityRegistry& lRegistry = InSubject.GetWorld()->GetRegistry();

                    if (!lEntry->Has(lRegistry, InSubject.GetHandle()))
                    {
                        return false;
                    }

                    const char* lName = lEntry->GetName().CStr();
                    InWidgets.PushId(lName);

                    if (InWidgets.CollapsingHeader(lName))
                    {
                        WidgetPropertyVisitor lVisitor(InWidgets);
                        lEntry->VisitProperties(lRegistry, InSubject.GetHandle(), lVisitor);
                    }

                    InWidgets.PopId();
                    return true;
                });
        });
    }

    void EditorService::RegisterNativeConfigDrawers()
    {
        // Engine and editor configs, through the same route a game's config would use. Drawn from their
        // OPAAX_PROPERTIES; only the resolver (which config) is config-specific.
        m_Extensions.ConfigDrawers().Register<Config_Engine>();
        m_Extensions.ConfigDrawers().Register<Config_Renderer>();
        m_Extensions.ConfigDrawers().Register<Config_EditorImgui, EditorImguiConfigDrawer>();
    }

    void EditorService::RegisterNativeResourceTypes()
    {
        // Both are set: the image shows, the glyph is the fallback if the image is missing.
        m_Extensions.ResourceTypes().Register<MapResource>()
            .SetIcon(OpaaxString("Icons/T_Map_Icon.png"))
            .SetGlyph(OpaaxString("[M]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_MAP_AT, InContext,
                                                        MapPathParams{InFile.AbsPath});
            });

        // Double-click places one into the focused map (also possible from the Hierarchy and a viewport drop).
        m_Extensions.ResourceTypes().Register<PrefabResource>()
            .SetGlyph(OpaaxString("[P]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_PREFAB_AT,
                                                        InContext, PrefabPathParams{InFile.AbsPath});
            });

        // Double-click opens the preview (the same route maps and levels use to open their document).
        m_Extensions.ResourceTypes().Register<TextureResource>()
            .SetIcon(OpaaxString("Icons/T_Texture_Icon.png"))
            .SetGlyph(OpaaxString("[T]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Preview.Open(InFile, ResourceTypeID::Get<TextureResource>());
                InContext.Panels.SetVisible(ResourcePreviewPanel::PanelID(), true);
            })
            .SetPreview<TextureResource>(&NativeResourcePreviews::DrawTexture);

        // A .ttf previews its baked atlas: glyph count, atlas size and metrics.
        m_Extensions.ResourceTypes().Register<FontFaceResource>()
            .SetGlyph(OpaaxString("[F]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Preview.Open(InFile, ResourceTypeID::Get<FontFaceResource>());
                InContext.Panels.SetVisible(ResourcePreviewPanel::PanelID(), true);
            })
            .SetPreview<FontFaceResource>(&NativeResourcePreviews::DrawFontFace);

        // A family opens its editor (it is a document).
        m_Extensions.ResourceTypes().Register<FontFamilyResource>()
            .SetGlyph(OpaaxString("[FF]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.FamilyDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(FontFamilyPanel::PanelID(), true);
                }
            });

        // A canvas opens its editor (it is a document).
        m_Extensions.ResourceTypes().Register<UICanvasResource>()
            .SetGlyph(OpaaxString("[UI]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                const UIWidgetRegistry& lRegistry =
                    OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets();

                if (InContext.UICanvasDocument.Open(InFile.AbsPath, lRegistry))
                {
                    InContext.Panels.SetVisible(UICanvasPanel::PanelID(), true);
                }
            });

        // A sheet opens its editor, not the preview (it is a document with its own panel).
        m_Extensions.ResourceTypes().Register<SpriteSheetResource>()
            .SetIcon(OpaaxString("Icons/T_SpriteSheet_Icon.png"))
            .SetGlyph(OpaaxString("[S]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.SheetDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(SpriteSheetPanel::PanelID(), true);
                }
            });

        // A clip opens its editor too.
        m_Extensions.ResourceTypes().Register<AnimationClipResource>()
            .SetGlyph(OpaaxString("[C]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.ClipDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(AnimationClipPanel::PanelID(), true);
                }
            });

        m_Extensions.ResourceTypes().Register<AnimationLibraryResource>()
            .SetGlyph(OpaaxString("[A]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.LibraryDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(AnimationLibraryPanel::PanelID(), true);
                }
            });

        // Both open their editor (they are documents).
        m_Extensions.ResourceTypes().Register<MoveModeResource>()
            .SetGlyph(OpaaxString("[MM]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.MoveModeDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(MoveModePanel::PanelID(), true);
                }
            });

        // Every data asset type opens the same panel; its fields come from the registered struct.
        m_Extensions.ResourceTypes().Register<DataAssetResource>()
            .SetGlyph(OpaaxString("[D]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.DataAssetDocument.Open(InFile.AbsPath, InContext.Engine.GetRegistries().DataAssets()))
                {
                    InContext.Panels.SetVisible(DataAssetPanel::PanelID(), true);
                }
            });

        m_Extensions.ResourceTypes().Register<MoverResource>()
            .SetGlyph(OpaaxString("[MV]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.MoverDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(MoverPanel::PanelID(), true);
                }
            });

        // Two types, two editors: an action is what gameplay binds, a context is which keys reach it
        // (a rebind only opens the context).
        m_Extensions.ResourceTypes().Register<InputActionResource>()
            .SetGlyph(OpaaxString("[IA]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.InputActionDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(InputActionPanel::PanelID(), true);
                }
            });

        m_Extensions.ResourceTypes().Register<InputMappingContextResource>()
            .SetGlyph(OpaaxString("[IM]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                if (InContext.InputMapDocument.Open(InFile.AbsPath))
                {
                    InContext.Panels.SetVisible(InputMappingContextPanel::PanelID(), true);
                }
            });

        m_Extensions.ResourceTypes().Register<LevelResource>()
            .SetIcon(OpaaxString("Icons/T_Level_Icon.png"))
            .SetGlyph(OpaaxString("[L]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext, LevelPathParams{InFile.AbsPath});
            });
    }
    
    // =============================================================================
    // =============================================================================
    // World
    // =============================================================================
    // =============================================================================
    
    bool EditorService::SetWorldManagerFromEngine(IEngine& InEngine)
    {
        m_WorldMgr  = &InEngine.GetWorldManager();
        
        bool lbIsValidWorldMgr = m_WorldMgr != nullptr;
        
        if (lbIsValidWorldMgr)
        {
            BindToWorldManagerDelegates();
        }
        
        return lbIsValidWorldMgr;
    }

    void EditorService::ClearWorldManager()
    {
        if (m_WorldMgr != nullptr)
        {
            UnbindFromWorldManagerDelegates();
            
            m_WorldMgr = nullptr;
        }else
        {
            //TODO logs.
        }
    }

    void EditorService::BindToWorldManagerDelegates()
    {
        OPAAX_ASSERT(m_WorldMgr != nullptr);
        
        m_WorldMgr->OnActiveWorldChanged.AddMember(this, &EditorService::HandleActiveWorldChanged);
        m_WorldMgr->OnWorldDestroyed.AddMember(this, &EditorService::HandleWorldDestroyed);
    }

    void EditorService::UnbindFromWorldManagerDelegates()
    {
        m_WorldMgr->OnActiveWorldChanged.RemoveAll(this);
        m_WorldMgr->OnWorldDestroyed.RemoveAll(this);
    }
    
    void EditorService::HandleActiveWorldChanged(World* InOld, World* InNew)
    {
        // Selection first, so no panel notified below reads one pointing into the old world.
        // Retargeted by guid (a clone keeps them), so the selection survives Play and Stop.
        if (m_Selection != nullptr && m_Selection->HasSelection())
        {
            // Every entry, in order. Guids are read before anything is cleared.
            World* const        lOldWorld = m_Selection->GetWorld();
            TDynArray<Guid>     lGuids;

            for (const EntityID lId : m_Selection->Ids())
            {
                lGuids.emplace_back(Entity{ lId, lOldWorld }.GetGuid());
            }

            m_Selection->Clear();

            // Entries without a counterpart are dropped (Entity holds a raw World*).
            for (const Guid& lGuid : lGuids)
            {
                if (InNew == nullptr) { break; }

                m_Selection->Add(InNew->FindByGuid(lGuid));   // Add ignores an invalid entity
            }
        }

        m_Gui->Panels().OnActiveWorldChanged(InOld, InNew);
    }

    void EditorService::HandleWorldDestroyed(World* InWorld)
    {
        // First: undo steps name entities of the edit world, so the history goes with it. Play/Stop
        // keeps the history (the Play copy is destroyed, not the edit world).
        if (m_Undo != nullptr && InWorld != nullptr && InWorld->GetMode() == EWorldMode::Edit)
        {
            m_Undo->Clear();
        }

        // The active world's death came through HandleActiveWorldChanged. This covers a non-active
        // world dying while it holds the selection.
        if (m_Selection == nullptr || !m_Selection->HasSelection() || InWorld == nullptr)
        {
            return;
        }

        // A selection is in one world, so one compare covers every entry.
        if (m_Selection->GetWorld() == InWorld)
        {
            m_Selection->Clear();
        }
    }
    
    // =============================================================================
    // =============================================================================
    // Level
    // =============================================================================
    // =============================================================================
    
    void EditorService::AdoptStartupLevel()
    {
        if (m_Context == nullptr || m_MapDocument == nullptr || m_LevelDocument == nullptr) { return; }

        // The engine already opened this level: read the manifest from the world's Level. Only the file
        // path comes from the project (Save needs it).
        const OpaaxString lLevelRel = OpaaxApplication::GetAppService<IProjectManager>().StartupLevel();

        LevelOps::AdoptOpen(*m_Context, lLevelRel.IsEmpty()
                                            ? OpaaxString()
                                            : m_Context->Paths.AssetToAbsolute(lLevelRel));
    }
    
    // =============================================================================
    // =============================================================================
    // GUI
    // =============================================================================
    // =============================================================================

    /**
     * Initializes the editor GUI.
     * @return False if it failed
     */
    bool EditorService::InitGUI(Window* InWindow)
    {
        if (!m_Gui->Init(*InWindow, ResolveLayoutIniPath()))
        {
            return false;
        }

        // After Init (no font atlas before) and before the first frame. The typeface comes from config.
        m_Gui->SetUIFont(ResolveUIFont());

        return true;
    }

    EditorUIFont EditorService::ResolveUIFont() const
    {
        IConfigSystem& lConfigSys = OpaaxApplication::GetAppService<IConfigSystem>();
        if (lConfigSys.IsNull())
        {
            return {};   // no config: keep the default font
        }

        const EditorImguiConfigData& lCFG = lConfigSys.Get<Config_EditorImgui>().GetData();

        // The config uses mount paths ("/Engine/Fonts/..."); the GUI needs absolute ones.
        IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();

        EditorUIFont lFont;
        lFont.SizePx = lCFG.UIFontSizePx;
        lFont.Path   = lCFG.UIFontPath.IsEmpty() ? OpaaxString() : lPaths.AssetToAbsolute(lCFG.UIFontPath);

        for (const OpaaxString& lFallback : lCFG.UIFontFallbacks)
        {
            if (!lFallback.IsEmpty())
            {
                lFont.Fallbacks.emplace_back(lPaths.AssetToAbsolute(lFallback));
            }
        }

        return lFont;
    }

    void EditorService::ClearGUI()
    {
        // Teardown: the gui owns the panels and destroys them before the backend.
        m_Gui->Teardown();
    }

    OpaaxString EditorService::ResolveLayoutIniPath() const
    {
        const EditorPaths* lEditorPaths = m_EditorPaths;
        if (lEditorPaths == nullptr)
        {
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths — dock layout will not persist.");
            return {};
        }

        // ImGui does not create directories and fails silently to save without one, so create it now.
        const OpaaxString lSaveDir = lEditorPaths->EditorSaveDir();
        const IFileSystem& lFileSystem = OpaaxApplication::GetAppService<IPlatform>().GetFileSystem();

        if (lFileSystem.GetPathIfNCreate(lSaveDir).IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn, "Could not create '{}' — dock layout will not persist.",
                      lSaveDir.CStr());
            return {};
        }

        return lEditorPaths->EditorToAbsolute(OpaaxString("Save/imgui.ini"));
    }

    // =============================================================================
    // =============================================================================
    // Panels
    // =============================================================================
    // =============================================================================
    
    void EditorService::BindPanelToggles()
    {
        for (const PanelEntry& lEntry : m_Extensions.Panels().Entries())
        {
            const PanelDesc& lDesc = lEntry.Desc;

            m_Extensions.TitleBar().Category(lDesc.Menu)
                        .AddCommand(lDesc.Id, Tags::EDITOR_COMMAND_TOGGLE_PANEL)
                        .SetParams(PanelIdParams{lDesc.Id})
                        .SetChecked([lId = lDesc.Id](const EditorContext& InContext)
                        {
                            return InContext.Panels.IsVisible(lId);
                        });
        }
    }

    void EditorService::BuildGUIs()
    {
        // Both live objects, each from its registry.
        m_Gui->TitleBar().Build(m_Extensions.TitleBar());
        m_Gui->Panels().Build(m_Extensions.Panels(), *m_Context);

        OPAAX_LOG(LogEditorService, Info,
                  "Editor UI built: title-bar entries={}, panels registered={}, constructed={}",
                  m_Extensions.TitleBar().Count(), m_Extensions.Panels().Count(), m_Gui->Panels().Count());

    }

    // =============================================================================
    // =============================================================================
    // Overrides
    // =============================================================================
    // =============================================================================
    
    void EditorService::RegisterAutomation(AutomationRunner& InRunner)
    {
        // No context: the editor did not start, so there is nothing to drive.
        if (m_Context == nullptr)
        {
            return;
        }
        EditorAutomation::Register(InRunner, *m_Context);
    }

    void EditorService::StopPlay()
    {
        if (m_PIE != nullptr && !m_PIE->IsEdit())
        {
            m_PIE->Stop();
        }
    }

    void EditorService::RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect)
    {
        RegisterNativePanels();
        RegisterNativeMenus();
        RegisterNativeResourceTypes();
        RegisterNativeEditorCommand();
        RegisterNativeDrawers();
        RegisterNativeConfigDrawers();

        // After the commands: the mode buttons dispatch by tag.
        RegisterNativeViewportTools();

        m_Extensions.EditWorldSystems().Bind(
            &OpaaxApplication::GetAppService<IEngine>().GetRegistries().WorldSubsystems());

        if (InCollect)
        {
            InCollect(m_Extensions);
        }

        // After the game module, so its own drawers win.
        RegisterGenericComponentDrawers();

        // After the game module (its panels get a toggle too), before sealing.
        BindPanelToggles();

        m_Extensions.Seal();

        OPAAX_LOG(LogEditorService, Info,
                  "Editor extensions sealed (before first world): drawers={}, configDrawers={}, panels={}, resourceTypes={}, titleBar={}, editWorldSystems={}, commands={}",
                  m_Extensions.Drawers().Count(), m_Extensions.ConfigDrawers().Count(),
                  m_Extensions.Panels().Count(), m_Extensions.ResourceTypes().Count(),
                  m_Extensions.TitleBar().Count(), m_Extensions.EditWorldSystems().Count(),
                  m_Extensions.Commands().Count());
    }

    void EditorService::Initialize()
    {
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        // Resolved before anything reads it (ResolveLayoutIniPath, then the EditorContext).
        CacheEditorPaths();
        
        IWindowManager& lWindows = OpaaxApplication::GetAppService<IWindowManager>();
        Window* lWindow = lWindows.GetMainWindow();
        if (lWindow == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "No main window at editor init — editor UI not created.");
            return;
        }

        // The editor draws its own title bar, so the OS must not. Set here: the window is created from
        // EngineConfigData alone.
        lWindow->SetDecorated(false);

        // No gui: the editor makes no sense without UI.
        if (!InitGUI(lWindow))
        {
            //TODO Log
            return;
        }
        
        // Many systems need the world: stop here.
        if (!SetWorldManagerFromEngine(lEngine))
        {
            //TODO Logs
            return;
        }
        
        CreateEditorSystems(lEngine);
        CreateEditorContext(lWindow, lEngine);
        
        PostInitialized();
    }

    void EditorService::BeginFrame()
    {
        if (!m_Gui->IsReady()) { return; }

        // Decide the input route once per frame, here: a rule evaluated only on events cannot notice that
        // input stopped (which is when the engine's held keys must be reset). Uses last frame's viewport
        // hover/focus, before OnPreRender clears it.
        if (m_InputRoute != nullptr) { m_InputRoute->Evaluate(); }

        m_Gui->BeginFrame();

        // Apply a pending viewport resize before Engine().Loop() renders, so this frame uses the new size.
        m_Gui->Panels().OnPreRender();
    }

    void EditorService::EndFrame()
    {
        if (!m_Gui->IsReady()) { return; }

        // Once per frame, before anything reads it (the Hierarchy shows a * per map).
        RefreshDirtyCache();

        DrawGUI();

        // Submit the UI after Engine().Loop() rendered the world into the FBO. The host presents after this.
        m_Gui->EndFrame();
    }

    bool EditorService::RouteInput(Event& InEvent)
    {
        if (!m_Gui->IsReady()) { return false; } // UI not up: pass through
        
        // Keyboard is not exempt: a text field that has the keyboard always wins.
        const bool lViewportHovered = m_InputRoute != nullptr && m_InputRoute->IsViewportHovered();

        bool lConsumed = false;

        // The editor feeds the game's pointer position (viewport-local, once per frame via InputRoute).
        // A raw window-pixel move must never reach the engine.
        if (InEvent.GetEventType() == MouseMovedEvent::GetStaticType())
        {
            return true;
        }

        if (InEvent.IsInCategory(EEventCategory::Mouse) || InEvent.IsInCategory(EEventCategory::MouseButton))
        {
            lConsumed = m_Gui->IsPointerOverUI() && !lViewportHovered;
        }
        else if (InEvent.IsInCategory(EEventCategory::Keyboard))
        {
            lConsumed = m_Gui->IsKeyboardOwnedByUI();
        }
        // Otherwise: window/application events (close, resize, ...) go to the base app.

        // Reserved editor keys, after the capture check: no shortcut while a text field has the keyboard.
        if (!lConsumed && HandleReservedKeys(InEvent))
        {
            return true;
        }

        // The route. Consuming here keeps the event from the engine (EditorApplication::OnEvent returns
        // early on true). Window events are exempt (close and resize belong to the application).
        if (!lConsumed && InEvent.IsInCategory(EEventCategory::Input)
            && m_InputRoute != nullptr && !m_InputRoute->IsOpen())
        {
            lConsumed = true;
        }

        // No log here (too noisy); the Input panel shows where input goes.
        return lConsumed;
    }

    bool EditorService::HandleReservedKeys(Event& InEvent)
    {
        if (m_Context == nullptr || InEvent.GetEventType() != KeyPressedEvent::GetStaticType())
        {
            return false;
        }

        const auto& lKey = static_cast<const KeyPressedEvent&>(InEvent);
        if (lKey.IsRepeat())
        {
            // Holding F7 must not stream steps.
            return false;
        }
        
        // Bare function keys: KeyPressed carries no modifier state, so Ctrl+P-style shortcuts are not possible yet.
        const OpaaxTag* lCommand = nullptr;
        switch (lKey.GetKeyCode())
        {
        case EKeyCode::F5: lCommand = &Tags::EDITOR_COMMAND_PLAY;
            break;
        case EKeyCode::F6: lCommand = &Tags::EDITOR_COMMAND_TOGGLE_PAUSE;
            break;
        case EKeyCode::F7: lCommand = &Tags::EDITOR_COMMAND_STEP;
            break;
        case EKeyCode::F8: lCommand = &Tags::EDITOR_COMMAND_STOP;
            break;
        default: return false;
        }

        OPAAX_LOG(LogEditorService, Info, "Reserved key -> {}", *lCommand);

        m_Context->Extensions.Commands().Execute(*lCommand, *m_Context);
        return true;
    }

    void EditorService::RefreshDirtyCache()
    {
        if (m_Context == nullptr || m_LevelDocument == nullptr)
        {
            return;
        }

        // Not while Play runs: the active world is the Play copy, and comparing a simulated world with the
        // authored baseline would mark every map dirty. The edit world does not change during Play.
        if (!m_Context->PIE.IsEdit()) { return; }

        const World* const lWorld = m_Context->Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        const Level* const lLevel = lWorld->GetLevel();
        if (lLevel == nullptr) { return; }

        // Throttled to 4 per second. The document skips the capture when the world revision has not moved,
        // so an idle editor is free; this limits the cost during a drag.
        constexpr double k_DirtyCheckInterval = 0.25;

        const double lNow = m_Gui->GetTime();
        if (lNow - m_LastDirtyCheck < k_DirtyCheckInterval) { return; }

        m_LastDirtyCheck = lNow;

        // The throttle is here (the frame clock); the answers are in the document (next to the baselines).
        m_LevelDocument->RefreshDirty(*lWorld, m_Context->Engine.GetRegistries().Components(), *lLevel);
    }

    void EditorService::HandleAuthoringShortcuts()
    {
        // Ctrl+S goes through ImGui, not HandleReservedKeys: with an Edit world open the input route
        // consumes every event, so InputManager never sees Ctrl. F5-F8 must work while the game has the
        // keyboard (so they are in the route); Ctrl+S only matters while the editor has it.
        if (m_Context == nullptr) { return; }

        // One chord for every document: the target follows the focused panel, which declares it
        // (PanelDesc::SaveCommand, Undo/Redo/DeleteCommand). Invalid = the level's command.
        const auto lCommandForFocused = [this](const OpaaxTag& InDefault, OpaaxTag PanelDesc::* InMember)
        {
            const OpaaxStringID lFocused = m_Gui->Panels().FocusedPanel();

            for (const PanelEntry& lEntry : m_Extensions.Panels().Entries())
            {
                if (lEntry.Desc.Id != lFocused) { continue; }

                const OpaaxTag& lDeclared = lEntry.Desc.*InMember;
                return lDeclared.IsValid() ? lDeclared : InDefault;
            }

            return InDefault;
        };

        if (m_Gui->Shortcut(EKeyCode::LeftControl, EKeyCode::S))
        {
            m_Context->Extensions.Commands().Execute(
                lCommandForFocused(Tags::EDITOR_COMMAND_SAVE_MAP, &PanelDesc::SaveCommand), *m_Context);
        }

        // Ctrl+Z / Ctrl+Y (Shortcut takes one modifier). Each is sampled once: Shortcut() is an edge query.
        if (m_Gui->Shortcut(EKeyCode::LeftControl, EKeyCode::Z))
        {
            m_Context->Extensions.Commands().Execute(
                lCommandForFocused(Tags::EDITOR_COMMAND_UNDO, &PanelDesc::UndoCommand), *m_Context);
        }

        if (m_Gui->Shortcut(EKeyCode::LeftControl, EKeyCode::Y))
        {
            m_Context->Extensions.Commands().Execute(
                lCommandForFocused(Tags::EDITOR_COMMAND_REDO, &PanelDesc::RedoCommand), *m_Context);
        }

        // F and Delete are editor-wide: their subject is the selection, which no panel owns. (Delete
        // follows the focused panel like the chords above; the prefab panel handles F itself.)
        // Guarded by IsKeyboardOwnedByUI, so typing in a text field does not trigger them.
        if (m_Gui->IsKeyboardOwnedByUI()) { return; }

        if (m_Gui->Shortcut(EKeyCode::F))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_FOCUS_SELECTED, *m_Context);
        }

        if (m_Gui->Shortcut(EKeyCode::Delete))
        {
            m_Context->Extensions.Commands().Execute(
                lCommandForFocused(Tags::EDITOR_COMMAND_DELETE_ENTITY, &PanelDesc::DeleteCommand), *m_Context);
        }

        // W / E / R (like Unreal, Unity and Godot). Editor-wide, like F and Delete; the keyboard guard
        // keeps typing safe. Edit only: in Play they are the game's movement keys. The menu entries
        // always work.
        if (!m_Context->PIE.IsEdit()) { return; }

        if (m_Gui->Shortcut(EKeyCode::W))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_TRANSLATE, *m_Context);
        }

        if (m_Gui->Shortcut(EKeyCode::E))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_ROTATE, *m_Context);
        }

        if (m_Gui->Shortcut(EKeyCode::R))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_SCALE, *m_Context);
        }
    }

    void EditorService::OnShutdown()
    {
        // Reverse-order teardown: EditorService is provided last, so it shuts down first, while the
        // engine, the window and its GL context are still alive.

        // 0. Unsubscribe from the WorldManager before the panels and selection its handlers use are destroyed.
        ClearWorldManager();

        // 1. The whole UI (panels, then backend): the Viewport frees its FBO while the GL context is
        //    current. Destroying the context also saves the dock layout. Before m_Context.reset().
        ClearGUI();

        // 4. Selection, PIE and the input route: after the panels that read them.
        ClearEditorSystems();

        // 5. The context references last.
        ClearEditorContext();
    }
}
