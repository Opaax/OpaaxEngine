#include "Editor/Application/Services/EditorService.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IProjectManager.h"
#include "Application/Services/Platforms/IFileSystem.h"
#include "Application/Services/Platforms/IPlatform.h"
#include "Application/Services/Window/IWindowManager.h"
#include "Core/Events/Event.h"
#include "Editor//Application/Services/EditorPaths.h"
#include "Editor/Commands/EditorNativeCommands.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include <cstdio>                              // snprintf — the pivot button's state-carrying label

#include "Editor/ImguiLibrary/ImguiWidgets.h"   // ToggleButton — the toolbar's mode and snap buttons
#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorViewport.hpp"   // the grid toggle lives on the viewport (③b)
#include "Editor/Operation/LevelOperations.h"
#include "Editor/Panels/ConfigPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/Panels/ResourcePreviewPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Renderer/Config/Config_Renderer.h"
#include "Engine/Subsystems/Input/InputEvents.h"
#include "World/Serialization/LevelResource.hpp"   // the types whose chrome is registered below
#include "World/Serialization/MapResource.hpp"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"   // the id the preview is opened with
#include "Engine/Subsystems/Resources/Types/TextureResource.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Entity/Entity.h"

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorService{"EditorService"};

    using namespace Opaax::Editor;

    /** True while the EDIT world is the one on screen — MapOps::CanEdit's rule, asked per frame. */
    bool IsEditing(const EditorContext& InContext) { return InContext.PIE.IsEdit(); }

    /** True while a Play clone is running or paused. */
    bool IsPlaying(const EditorContext& InContext) { return !InContext.PIE.IsEdit(); }

    bool IsPaused(const EditorContext& InContext) { return InContext.PIE.IsPaused(); }

    /** Below this a snap step collapses every drag onto one point, so the toolbar clamps to it. */
    constexpr float k_MinSnapStep = 0.001f;
}

namespace Opaax::Editor
{
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
        m_Viewport          = MakeUnique<EditorViewport>();
        m_Preview           = MakeUnique<ResourcePreview>();
        m_Camera            = MakeUnique<EditorCamera>();
        m_Gizmo             = MakeUnique<EditorGizmo>();
        m_PIE               = MakeUnique<PlayInEditor>(*m_WorldMgr);
        m_InputRoute        = MakeUnique<InputRoute>(*m_WorldMgr, InEngine.GetInput(), *m_PIE);
        m_MapDocument       = MakeUnique<EditorMapDocument>();
        m_LevelDocument     = MakeUnique<EditorLevelDocument>();
        m_EditorPanels         = MakeUnique<EditorPanels>();
    }

    void EditorService::ClearEditorSystems()
    {
        m_Selection.reset();
        m_Viewport.reset();
        m_Preview.reset();
        m_Camera.reset();
        m_Gizmo.reset();
        m_InputRoute.reset();
        m_PIE.reset();
        m_EditorPanels.reset();
    }

    void EditorService::CreateEditorContext(Window* InWindow, IEngine& InEngine)
    {
        // --- EditorContext: Built after the UIBackend so it can hold a reference to it. ---------------------------------------
        m_Context = MakeUnique<EditorContext>(EditorContext{
            InEngine,
            InEngine.GetWorldManager(),
            InEngine.GetResources(),
            m_Gui.Backend(),
            *m_Selection,
            *m_Viewport,
            *m_Camera,
            *m_Gizmo,
            *m_PIE,
            *m_InputRoute,
            *m_LevelDocument,
            *m_MapDocument,
            m_Extensions,
            *m_EditorPanels,
            *m_Preview,
            OpaaxApplication::GetAppService<IPaths>(),
            OpaaxApplication::GetAppService<IPlatform>().GetFileSystem(),
            OpaaxApplication::GetAppService<IConfigSystem>(),
            *InWindow,
            m_EditorPaths
        });
    }

    void EditorService::ClearEditorContext()
    {
        m_Context.reset();
    }

    void EditorService::PostInitialized()
    {
        BuildPanels();
        AdoptStartupLevel();
    }
    
    void EditorService::DrawGUI()
    {
        if (m_Context != nullptr)
        {
            m_Gui.Draw(*m_Context);
            //Should be drawned from GUI
            m_Extensions.Menus().Draw(*m_Context);
        }

        HandleAuthoringShortcuts();
        
        // The Viewport panel samples the FBO the world was just rendered into (Engine().Loop() above)
        // and shows it as an ImGui image — the world lives INSIDE a panel now, not the raw backbuffer.
        // It is the first panel in the host, with no special path of its own.
        if (m_EditorPanels != nullptr)
        {
            m_EditorPanels->Draw();
        }
    }

    // =============================================================================
    // =============================================================================
    // Editor Registers 
    // =============================================================================
    // =============================================================================
    
    void EditorService::RegisterNativeMenus()
    {
        EditorMenu& lMenu = m_Extensions.Menus();

        // --- Native File  --------------------------
        EditorMenuCategory& lFile = lMenu.Category("File");
        lFile.AddCommand("New Map...", Tags::EDITOR_COMMAND_NEW_MAP).SetEnabled(IsEditing);
        lFile.AddCommand("Open Map...", Tags::EDITOR_COMMAND_OPEN_MAP);
        lFile.AddSeparator();
        lFile.AddCommand("Save Map", Tags::EDITOR_COMMAND_SAVE_MAP).SetEnabled(IsEditing);
        lFile.AddCommand("Save Map As...", Tags::EDITOR_COMMAND_SAVE_MAP_AS).SetEnabled(IsEditing);
        lFile.AddSeparator();
        lFile.AddCommand("Open Level...", Tags::EDITOR_COMMAND_OPEN_LEVEL);
        lFile.AddCommand("Save Level", Tags::EDITOR_COMMAND_SAVE_LEVEL).SetEnabled(IsEditing);
        lFile.AddSeparator();
        lFile.AddCommand("Exit", Tags::EDITOR_COMMAND_QUIT);

        // --- Native Edit  --------------------------
        // Enabled only while editing: all three are refused in a Play world anyway, and a menu that
        // states the rule beats one that answers a click with a log line nobody reads.
        EditorMenuCategory& lEdit = lMenu.Category("Edit");
        lEdit.AddCommand("Create Entity", Tags::EDITOR_COMMAND_CREATE_ENTITY).SetEnabled(IsEditing);
        lEdit.AddCommand("Delete Selected", Tags::EDITOR_COMMAND_DELETE_ENTITY).SetEnabled(IsEditing);
        lEdit.AddSeparator();
        lEdit.AddCommand("Focus Selected", Tags::EDITOR_COMMAND_FOCUS_SELECTED).SetEnabled(IsEditing);

        // --- Gizmo mode (③), as radio entries -----------------------------------------------
        // The tick reads the live mode rather than a remembered one, so the menu and the W/E/R keys
        // cannot disagree — the same reason BindPanelToggles reads EditorPanels. Discoverability is
        // the whole point: a mode reachable only by a key nobody documented is folklore.
        lEdit.AddSeparator();
        lEdit.AddCommand("Gizmo: Translate (W)", Tags::EDITOR_COMMAND_GIZMO_TRANSLATE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Translate; });
        lEdit.AddCommand("Gizmo: Rotate (E)", Tags::EDITOR_COMMAND_GIZMO_ROTATE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Rotate; });
        lEdit.AddCommand("Gizmo: Scale (R)", Tags::EDITOR_COMMAND_GIZMO_SCALE)
             .SetChecked([](const EditorContext& InContext) { return InContext.Gizmo.GetMode() == EGizmoMode::Scale; });

        // --- Native Level  --------------------------
        EditorMenuCategory& lLevel = lMenu.Category("Level");
        lLevel.AddCommand("Add Map...", Tags::EDITOR_COMMAND_ADD_MAP_TO_LEVEL).SetEnabled(IsEditing);

        // --- Native Play  --------------------------
        EditorMenuCategory& lPlay = lMenu.Category("Play");
        lPlay.AddCommand("Play", Tags::EDITOR_COMMAND_PLAY).SetEnabled(IsEditing);
        lPlay.AddCommand("Pause", Tags::EDITOR_COMMAND_TOGGLE_PAUSE).SetChecked(IsPaused).SetEnabled(IsPlaying);
        lPlay.AddCommand("Step", Tags::EDITOR_COMMAND_STEP).SetEnabled(IsPaused);
        lPlay.AddSeparator();
        lPlay.AddCommand("Stop", Tags::EDITOR_COMMAND_STOP).SetEnabled(IsPlaying);
    }
    
    void EditorService::RegisterNativePanels()
    {
        PanelRegistry& lPanelsRegistry = m_Extensions.Panels();
        
        //Visible by default
        lPanelsRegistry.Register<ViewportPanel>(PanelDesc       {.Id = ViewportPanel::PanelID()});
        lPanelsRegistry.Register<PlayToolbarPanel>(PanelDesc    {.Id = PlayToolbarPanel::PanelID()});
        lPanelsRegistry.Register<HierarchyPanel>(PanelDesc      {.Id = HierarchyPanel::PanelID()});
        lPanelsRegistry.Register<InspectorPanel>(PanelDesc      {.Id = InspectorPanel::PanelID()});
        lPanelsRegistry.Register<ResourceBrowserPanel>(PanelDesc{.Id = ResourceBrowserPanel::PanelID()});
        
        //Hidden by default
        lPanelsRegistry.Register<ResourcePreviewPanel>(PanelDesc{.Id = ResourcePreviewPanel::PanelID(), .DefaultVisibility = EPanelVisibility::Hidden});
        lPanelsRegistry.Register<ConfigPanel>(PanelDesc         {.Id = ConfigPanel::PanelID(),          .DefaultVisibility = EPanelVisibility::Hidden});
        lPanelsRegistry.Register<InputPanel>(PanelDesc          {.Id = InputPanel::PanelID(),           .DefaultVisibility = EPanelVisibility::Hidden });
    }

    void EditorService::RegisterNativeEditorCommand()
    {
        EditorCommandRegistry& lCommands = m_Extensions.Commands();

        lCommands.Register<QuitCommand>(Tags::EDITOR_COMMAND_QUIT);
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
        lCommands.Register<SaveMapCommand>(Tags::EDITOR_COMMAND_SAVE_MAP);
        lCommands.Register<SaveMapAsCommand>(Tags::EDITOR_COMMAND_SAVE_MAP_AS);

        lCommands.Register<OpenLevelCommand>(Tags::EDITOR_COMMAND_OPEN_LEVEL);
        lCommands.Register<OpenLevelAtCommand>(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT);
        lCommands.Register<SaveLevelCommand>(Tags::EDITOR_COMMAND_SAVE_LEVEL);
        lCommands.Register<AddMapToLevelCommand>(Tags::EDITOR_COMMAND_ADD_MAP_TO_LEVEL);
    }

    void EditorService::RegisterNativeViewportTools()
    {
        ViewportToolbarRegistry& lTools = m_Extensions.ViewportTools();

        // --- Gizmo mode ---------------------------------------------------------------------
        // BY TAG, so this is a THIRD front-end onto the same commands the Edit menu and W/E/R use.
        // Calling EditorGizmo::SetMode directly here would be a fourth place to keep correct.
        lTools.Add(OPAAX_ID("GizmoMode"), [](EditorContext& InContext)
        {
            struct ModeEntry { const char* Label; EGizmoMode Mode; const OpaaxTag& Command; };

            const ModeEntry lModes[] = {
                { "Move",   EGizmoMode::Translate, Tags::EDITOR_COMMAND_GIZMO_TRANSLATE },
                { "Rotate", EGizmoMode::Rotate,    Tags::EDITOR_COMMAND_GIZMO_ROTATE },
                { "Scale",  EGizmoMode::Scale,     Tags::EDITOR_COMMAND_GIZMO_SCALE },
            };

            bool bFirst = true;
            for (const ModeEntry& lEntry : lModes)
            {
                if (!bFirst) { ImGui::SameLine(); }
                bFirst = false;

                if (ImguiWidgets::ToggleButton(lEntry.Label, InContext.Gizmo.GetMode() == lEntry.Mode))
                {
                    InContext.Extensions.Commands().Execute(lEntry.Command, InContext);
                }
            }
        });

        lTools.AddSeparator();

        // --- Snapping -----------------------------------------------------------------------
        // The toggle and the STEP together: a toggle over a number you cannot change is half a
        // control, and the step is what an author actually tunes per map.
        lTools.Add(OPAAX_ID("Snap"), [](EditorContext& InContext)
        {
            EditorGizmo& lGizmo = InContext.Gizmo;

            if (ImguiWidgets::ToggleButton("Snap", lGizmo.IsSnapEnabled()))
            {
                lGizmo.SetSnapEnabled(!lGizmo.IsSnapEnabled());
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Snap the gizmo to fixed steps.\nHold Ctrl to invert this while dragging.");
            }

            // The step for the ACTIVE mode only — three fields at once would be a settings popup,
            // and the one an author wants is always the one they are about to drag with.
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.f);

            const EGizmoMode lMode = lGizmo.GetMode();

            // Degrees for rotate, a fraction for scale, world units otherwise — the format says
            // which, so the number is never ambiguous.
            const char* lFormat = lMode == EGizmoMode::Rotate ? "%.0f deg"
                                : lMode == EGizmoMode::Scale  ? "%.2f x"
                                                              : "%.1f u";

            float& lStep = lGizmo.SnapStepRef(lMode);
            if (ImGui::DragFloat("##step", &lStep, lMode == EGizmoMode::Scale ? 0.01f : 0.5f,
                                 0.f, 0.f, lFormat))
            {
                // A zero or negative step would make ImGuizmo snap everything onto one point.
                lStep = lStep < k_MinSnapStep ? k_MinSnapStep : lStep;
            }
        });

        // --- Grid ---------------------------------------------------------------------------
        // Beside the snap controls WITHOUT a separator, because it is one of them: the grid's
        // spacing IS the translate step above, so the two belong in the same group.
        lTools.Add(OPAAX_ID("Grid"), [](EditorContext& InContext)
        {
            EditorViewport& lViewport = InContext.Viewport;

            if (ImguiWidgets::ToggleButton("Grid", lViewport.IsGridVisible()))
            {
                lViewport.SetShowGrid(!lViewport.IsGridVisible());
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Show a grid at the TRANSLATE snap step.\n"
                                  "It coarsens by decades as you zoom out.");
            }
        });

        lTools.AddSeparator();

        // --- Pivot --------------------------------------------------------------------------
        // One button that NAMES ITS CURRENT STATE rather than a pair of radio buttons: there are
        // only two values, so the label is the readout and clicking is the toggle.
        lTools.Add(OPAAX_ID("Pivot"), [](EditorContext& InContext)
        {
            EditorGizmo& lGizmo = InContext.Gizmo;

            // THREE states, so it cycles rather than toggles. The label is the readout.
            const EGizmoPivot lPivot = lGizmo.GetPivot();

            // "###pivot" pins the ImGui ID to the part after it, so a label that changes with the
            // state does not make this a different widget every time it is clicked.
            char lLabel[48];
            std::snprintf(lLabel, sizeof(lLabel), "Pivot: %s###pivot", ToString(lPivot));

            if (ImGui::SmallButton(lLabel))
            {
                lGizmo.SetPivot(lPivot == EGizmoPivot::Center     ? EGizmoPivot::Origin
                              : lPivot == EGizmoPivot::Origin     ? EGizmoPivot::Individual
                                                                  : EGizmoPivot::Center);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Center — one point, the selection's combined bounds.\n"
                                  "Origin — one point, the last-picked entity's position.\n"
                                  "Individual — each entity turns about ITSELF; nothing orbits.\n"
                                  "All three agree for a single entity.");
            }
        });

        // --- Space --------------------------------------------------------------------------
        lTools.Add(OPAAX_ID("Space"), [](EditorContext& InContext)
        {
            EditorGizmo& lGizmo = InContext.Gizmo;

            // SCALE FORCES LOCAL, so the button says so and refuses rather than lying. A world-axis
            // non-uniform scale of a rotated entity is a SHEAR, which TransformComponent cannot hold.
            const bool bForced = lGizmo.GetMode() == EGizmoMode::Scale;
            const bool bLocal  = lGizmo.GetEffectiveSpace() == EGizmoSpace::Local;

            ImGui::BeginDisabled(bForced);

            if (ImGui::SmallButton(bLocal ? "Space: Local###space" : "Space: World###space"))
            {
                lGizmo.SetSpace(bLocal ? EGizmoSpace::World : EGizmoSpace::Local);
            }

            ImGui::EndDisabled();

            // OUTSIDE BeginDisabled: a disabled item does not report hover, and the one moment the
            // tooltip is most needed is when the button will not move.
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip(bForced
                    ? "Scale is always Local — scaling a rotated entity along world axes\n"
                      "is a shear, which a transform cannot represent."
                    : "Local — handles follow the last-picked entity's rotation.\n"
                      "World — handles stay axis-aligned.");
            }
        });
    }

    void EditorService::RegisterNativeConfigDrawers()
    {
        // The engine's own configs, through the SAME route and the same registry template a game's
        // config would use. Both draw from their data type's OPAAX_PROPERTIES — there is no
        // config-shaped drawer code anywhere, only the resolver that says which config a drawer is
        // for.
        m_Extensions.ConfigDrawers().Register<Config_Engine>();
        m_Extensions.ConfigDrawers().Register<Config_Renderer>();
    }

    void EditorService::RegisterNativeResourceTypes()
    {
        // The glyph and the image are BOTH set: the image is what shows, the glyph is what shows if
        // it cannot be found. Neither names an extension — that label comes from the FORMAT,
        // engine-side.
        m_Extensions.ResourceTypes().Register<MapResource>()
            .SetIcon(OpaaxString("Icons/T_Map_Icon.png"))
            .SetGlyph(OpaaxString("[M]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_MAP_AT, InContext,
                                                        MapPathParams{InFile.AbsPath});
            });

        // Double-click OPENS THE PREVIEW, through the seam that already answers "what does a
        // double-click do" — the same one Map and Level use to open a document. That is why the
        // Inspector's TPropertyDrawer contract did not have to grow an EditorContext to get a
        // texture preview: the preview lives where a context already is (I15 untouched).
        m_Extensions.ResourceTypes().Register<TextureResource>()
            .SetIcon(OpaaxString("Icons/T_Texture_Icon.png"))
            .SetGlyph(OpaaxString("[T]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Preview.Open(InFile, ResourceTypeID::Get<TextureResource>());
                InContext.Panels.SetVisible(ResourcePreviewPanel::PanelID(), true);
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
        // Selection FIRST, so no panel notified below can read one pointing into the old world.
        //
        // Retarget rather than clear: a clone preserves entity GUIDs (WM3), so the entity selected in
        // Edit has a counterpart in the Play world and the selection survives Play AND Stop. Clearing
        // would be safe too, but it would throw away the exact guarantee the snapshot core exists for.
        if (m_Selection != nullptr && m_Selection->HasSelection())
        {
            // EVERY entry, in order, so a multi-selection survives Play and Stop exactly as a single
            // one does. The Guids are read BEFORE anything is cleared — they are the only thing that
            // means anything across the two worlds.
            World* const        lOldWorld = m_Selection->GetWorld();
            TDynArray<Guid>     lGuids;

            for (const EntityID lId : m_Selection->Ids())
            {
                lGuids.emplace_back(Entity{ lId, lOldWorld }.GetGuid());
            }

            m_Selection->Clear();

            // Whatever has no counterpart is DROPPED rather than kept: Entity holds a raw World*, so
            // a survivor of the old world would dangle the moment it dies. An entity destroyed during
            // play simply leaves the selection, and the rest of it stays.
            for (const Guid& lGuid : lGuids)
            {
                if (InNew == nullptr) { break; }

                m_Selection->Add(InNew->FindByGuid(lGuid));   // Add ignores an invalid entity
            }
        }

        if (m_EditorPanels != nullptr)
        {
            m_EditorPanels->OnActiveWorldChanged(InOld, InNew);
        }
    }

    void EditorService::HandleWorldDestroyed(World* InWorld)
    {
        // The active world's death already came through HandleActiveWorldChanged (DestroyWorld clears
        // the active slot first). This covers the other case — a NON-active world dying while holding
        // the selection, which nothing else would notice.
        if (m_Selection == nullptr || !m_Selection->HasSelection() || InWorld == nullptr)
        {
            return;
        }

        // One world per selection by construction, so one compare covers every entry.
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

        // The engine already opened this level into the world (FinishStartup -> OpenLevel), so the
        // manifest is READ OFF THE WORLD'S LEVEL rather than re-read from the file: the Level IS
        // the "what did I load" answer the editor used to have to reconstruct. Only the file's
        // PATH still comes from the project, because a Save needs somewhere to write.
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
     * Init the editor GUI
     * @param InWindow 
     * @return false if not initialized correctly
     */
    bool EditorService::InitGUI(Window* InWindow)
    {
        return m_Gui.Init(*InWindow, ResolveLayoutIniPath());
    }

    void EditorService::ClearGUI()
    {
        m_Gui.Shutdown();
    }

    OpaaxString EditorService::ResolveLayoutIniPath() const
    {
        const EditorPaths* lEditorPaths = m_EditorPaths;
        if (lEditorPaths == nullptr)
        {
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths — dock layout will not persist.");
            return {};
        }

        // ImGui does not create directories, and its save fails SILENTLY when one is missing — so the dir
        // has to exist before the first write, not on first save. GetPathIfNCreate is exactly that, and
        // logs its own failure detail.
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

            m_Extensions.Menus().Category(lDesc.Menu)
                        .AddCommand(lDesc.Id, Tags::EDITOR_COMMAND_TOGGLE_PANEL)
                        .SetParams(PanelIdParams{lDesc.Id})
                        .SetChecked([lId = lDesc.Id](const EditorContext& InContext)
                        {
                            return InContext.Panels.IsVisible(lId);
                        });
        }
    }

    void EditorService::BuildPanels()
    {
        m_EditorPanels->Build(m_Extensions.Panels(), *m_Context);
        OPAAX_LOG(LogEditorService, Info, "Editor panels registered: {}, constructed: {}", m_Extensions.Panels().Count(), m_EditorPanels->Count());
    }

    void EditorService::ClearPanels()
    {
        if (m_EditorPanels != nullptr)
        {
            m_EditorPanels->Shutdown();
        }
    }

    // =============================================================================
    // =============================================================================
    // Overrides
    // =============================================================================
    // =============================================================================
    
    void EditorService::RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect)
    {
        RegisterNativePanels();
        RegisterNativeMenus();
        RegisterNativeResourceTypes();
        RegisterNativeEditorCommand();
        RegisterNativeConfigDrawers();

        // AFTER the commands, because the mode buttons dispatch by tag and a toolbar registered
        // ahead of them would name commands that do not exist yet.
        RegisterNativeViewportTools();

        m_Extensions.EditWorldSystems().Bind(
            &OpaaxApplication::GetAppService<IEngine>().GetRegistries().WorldSubsystems());

        if (InCollect)
        {
            InCollect(m_Extensions);
        }

        // AFTER the game module, so its panels get a toggle too, and before the seal.
        BindPanelToggles();

        m_Extensions.Seal();

        OPAAX_LOG(LogEditorService, Info,
                  "Editor extensions sealed (before first world): drawers={}, configDrawers={}, panels={}, resourceTypes={}, menus={}, editWorldSystems={}, commands={}",
                  m_Extensions.Drawers().Count(), m_Extensions.ConfigDrawers().Count(),
                  m_Extensions.Panels().Count(), m_Extensions.ResourceTypes().Count(),
                  m_Extensions.Menus().Count(), m_Extensions.EditWorldSystems().Count(),
                  m_Extensions.Commands().Count());
    }

    void EditorService::Initialize()
    {
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        // Resolved before anything reads it: ResolveLayoutIniPath below, then the EditorContext.
        CacheEditorPaths();
        
        IWindowManager& lWindows = OpaaxApplication::GetAppService<IWindowManager>();
        Window* lWindow = lWindows.GetMainWindow();
        if (lWindow == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "No main window at editor init — editor UI not created.");
            return;
        }

        // No gui means editor without ui so it make no sense init 
        if (!InitGUI(lWindow))
        {
            //TODO Log
            return;
        }
        
        // Many systems rely on world so do not continue the init
        if (!SetWorldManagerFromEngine(lEngine))
        {
            //TODO Logs
            return;
        }
        
        CreateEditorSystems(lEngine);
        CreateEditorContext(lWindow, lEngine);
        
        OPAAX_LOG(LogEditorService, Info, "EditorService initialized");
        
        PostInitialized();
    }

    void EditorService::BeginFrame()
    {
        if (!m_Gui.IsReady()) { return; }

        // Re-decide the input route ONCE per frame, here rather than inside RouteInput: a rule
        // evaluated only when an event arrives cannot notice that input STOPPED — and "the route
        // just closed" is precisely the case that has to reset the engine's held keys. Reads the
        // viewport hover/focus the panel pushed last frame, BEFORE OnPreRender clears it.
        if (m_InputRoute != nullptr) { m_InputRoute->Evaluate(); }

        m_Gui.BeginFrame();

        // Apply any pending viewport resize (measured last DrawContents) BEFORE Engine().Loop()
        // renders the world, so Render() reads the new FBO size this frame (deferred-resize
        // handshake, §5).
        if (m_EditorPanels != nullptr) { m_EditorPanels->OnPreRender(); }
    }

    void EditorService::EndFrame()
    {
        if (!m_Gui.IsReady()) { return; }

        // Once per frame, ahead of everything that reads it — the Hierarchy draws a `*` per map.
        RefreshDirtyCache();

        DrawGUI();

        // Submit the UI to the backbuffer AFTER Engine().Loop() has rendered the world into the FBO
        // (see EditorApplication::TickFrame). The host presents the backbuffer once, after this.
        m_Gui.EndFrame();
    }

    bool EditorService::RouteInput(Event& InEvent)
    {
        if (!m_Gui.IsReady()) { return false; } // UI not up (pre-Initialize / no window) — pass through
        
        // Keyboard is NOT exempted. IsKeyboardOwnedByUI only goes true for a text field, and a
        // field that has the keyboard must always win, viewport or not.
        const bool lViewportHovered = m_InputRoute != nullptr && m_InputRoute->IsViewportHovered();

        bool lConsumed = false;
        if (InEvent.IsInCategory(EEventCategory::Mouse) || InEvent.IsInCategory(EEventCategory::MouseButton))
        {
            lConsumed = m_Gui.IsPointerOverUI() && !lViewportHovered;
        }
        else if (InEvent.IsInCategory(EEventCategory::Keyboard))
        {
            lConsumed = m_Gui.IsKeyboardOwnedByUI();
        }
        // else: window/application events (close, resize, ...) always fall through to the base app.

        // Step 3 — the reserved editor keys, AFTER the capture check on purpose: a shortcut must not
        // fire while a text field owns the keyboard, and D5 orders it exactly this way.
        if (!lConsumed && HandleReservedKeys(InEvent))
        {
            return true;
        }

        // Steps 2 + 4 — the route. Consuming here is what withholds the event from the engine:
        // EditorApplication::OnEvent returns early on true, so "the editor ate it" and "the engine
        // never saw it" are the same statement, and there is no second gate downstream to keep in
        // sync. Window events are exempt — close and resize are the application's business no
        // matter where the pointer is.
        if (!lConsumed && InEvent.IsInCategory(EEventCategory::Input)
            && m_InputRoute != nullptr && !m_InputRoute->IsOpen())
        {
            lConsumed = true;
        }

        // Observability for the seam (Trace only, discrete events — never per mouse-move, so no spam).
        // Over the UI -> CONSUMED; over the passthru viewport -> passed to engine.
        if (InEvent.IsInCategory(EEventCategory::MouseButton) || InEvent.IsInCategory(EEventCategory::Keyboard))
        {
            OPAAX_LOG(LogEditorService, Trace, "RouteInput: {} -> {} (PointerOverUI={}, KeyboardOwnedByUI={})",
                      InEvent.GetName(), lConsumed ? "CONSUMED by editor" : "passed to engine",
                      m_Gui.IsPointerOverUI(), m_Gui.IsKeyboardOwnedByUI());
        }

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
            // Holding F7 must not stream steps; every PIE verb is a discrete command.
            return false;
        }
        
        // Bare function keys, not chords: the KeyPressed payload carries no modifier state, so
        // Ctrl+P-style shortcuts are not expressible today. M-Input owns that.
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

        // NOT WHILE PIE RUNS — the same rule MapOps::CanEdit applies, for the same reason. The
        // active world is then the Play CLONE, whose entities carry the source's OwnerMap
        // (MapFactory restores it) and whose Level adopted the source's mounts. Checking it would
        // compare a world being SIMULATED against the authored baseline: every map goes dirty the
        // moment the game moves anything, and the capture is paid on the frames that can least
        // afford it. The edit world is untouched throughout, so there is nothing to re-derive.
        if (!m_Context->PIE.IsEdit()) { return; }

        const World* const lWorld = m_Context->Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        const Level* const lLevel = lWorld->GetLevel();
        if (lLevel == nullptr) { return; }

        // THROTTLED, and it now COALESCES rather than bounds. The document skips the whole capture
        // when the world's revision has not moved (MP5), so an idle editor is already free; what is
        // left to bound is a DRAG, which bumps the revision every frame it is held. Four times a
        // second is far below what an eye can tell from instant.
        constexpr double k_DirtyCheckInterval = 0.25;

        const double lNow = m_Gui.GetTime();
        if (lNow - m_LastDirtyCheck < k_DirtyCheckInterval) { return; }

        m_LastDirtyCheck = lNow;

        // The THROTTLE lives here because this is where the frame clock is; the ANSWERS live in the
        // document, beside the records they are derived from, so the Hierarchy can read one per map
        // without a capture (**MP5**).
        m_LevelDocument->RefreshDirty(*lWorld, m_Context->Engine.GetRegistries().Components(), *lLevel);
    }

    void EditorService::HandleAuthoringShortcuts()
    {
        // Ctrl+S goes through IMGUI, not through HandleReservedKeys — and the reason is worth
        // keeping. D5's step 3 runs inside the event route, where the editor decides whether the
        // ENGINE gets fed. With an Edit world open the route is ClosedEditMode, so every input
        // event is consumed there and InputManager never sees Ctrl at all: IsCtrlDown() would be
        // false forever, precisely where Ctrl+S is wanted.
        //
        // The split is principled rather than a workaround. F5-F8 are PIE control and must fire
        // while the GAME owns the keyboard, so they belong in the route ahead of the feed. Ctrl+S
        // is an authoring command that only means anything while the EDITOR owns the keyboard —
        // which is exactly when ImGui's view of the keyboard is the authoritative one.
        if (m_Context == nullptr) { return; }

        if (m_Gui.Shortcut(EKeyCode::LeftControl, EKeyCode::S))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_MAP, *m_Context);
        }

        // F and Delete are EDITOR-WIDE, not the viewport's. They were measured on the viewport
        // first, which meant they did nothing from the Hierarchy — the panel an author is most
        // likely to be in when deleting something. Their subject is the SELECTION, and the
        // selection is not owned by any one panel, so neither are its verbs.
        //
        // What made a bare key unsafe was never the route, it was a text field: guarding on
        // IsKeyboardOwnedByUI is what lets these be global, so typing "Fred" into the name field
        // cannot frame and delete the selection.
        if (m_Gui.IsKeyboardOwnedByUI()) { return; }

        if (m_Gui.Shortcut(EKeyCode::F))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_FOCUS_SELECTED, *m_Context);
        }

        if (m_Gui.Shortcut(EKeyCode::Delete))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_DELETE_ENTITY, *m_Context);
        }

        // W / E / R — the binding Unreal, Unity and Godot all share, so an author already knows it.
        // Editor-wide beside F and Delete for SEL7's reason: the gizmo's subject is the SELECTION,
        // and the selection belongs to no single panel. The WantCaptureKeyboard guard above is what
        // keeps a bare letter safe — typing "Water" into a name field must not switch modes.
        //
        // EDIT ONLY, unlike F and Delete: W/E/R are also the game's movement keys, and a running
        // game owns them. The menu entries stay live either way — they are unambiguous, and a mode
        // set ahead of Stop is a reasonable thing to want.
        if (!m_Context->PIE.IsEdit()) { return; }

        if (m_Gui.Shortcut(EKeyCode::W))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_TRANSLATE, *m_Context);
        }

        if (m_Gui.Shortcut(EKeyCode::E))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_ROTATE, *m_Context);
        }

        if (m_Gui.Shortcut(EKeyCode::R))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_SCALE, *m_Context);
        }
    }

    void EditorService::OnShutdown()
    {
        // Reverse-order teardown: EditorService is provided last, so this runs FIRST — the engine, the
        // window and its GL context are all still alive (LC). Order within:

        // 0. Unsubscribe while the WorldManager is still alive, and BEFORE the panels/selection those
        //    handlers touch are destroyed — WorldManager::TearDown destroys every world and would
        //    otherwise call back into a half-torn-down editor.
        ClearWorldManager();

        // 1. Every panel, reverse construction order (LC3). The Viewport registered first so it dies
        //    LAST, which is the right end: its Shutdown clears the engine's primary render target
        //    while the engine is alive (no live frame reads a dangling target) and frees the FBO
        //    while the GL context is still current — both true here, since the UI backend below has
        //    not gone yet. All of it must precede m_Context.reset(): panels hold a reference into it.
        ClearPanels();

        // 3. The UI stack — the renderer impl's shutdown requires the GL context, still alive here,
        //    and destroying the context is what FLUSHES the dock layout. EditorGui owns both halves
        //    and the ordering between them.
        ClearGUI();

        // 4. Selection, PIE and the input route — after the panels that read them, before the
        //    context they are referenced from. All three hold only non-owning references, so there
        //    is nothing to undo; the route is dropped before the engine it would reset.
        ClearEditorSystems();

        // 5. The context refs last (nothing points into them anymore).
        ClearEditorContext();
        
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
