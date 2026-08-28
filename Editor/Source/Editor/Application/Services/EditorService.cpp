#include "Editor/Application/Services/EditorService.h"

#include <imgui.h>
#include <ImGuizmo.h>   // BeginFrame — the gizmo's per-frame reset (③)

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
#include "Editor/Operation/LevelOperations.h"
#include "Editor/Panels/ConfigPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/Panels/ResourcePreviewPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/UI/OpenGLEditorUIBackend.h"
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
}

namespace Opaax::Editor
{
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
                InContext.Panels.SetVisible(PreviewPanelId(), true);
            });

        m_Extensions.ResourceTypes().Register<LevelResource>()
            .SetIcon(OpaaxString("Icons/T_Level_Icon.png"))
            .SetGlyph(OpaaxString("[L]"))
            .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
            {
                InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext, LevelPathParams{InFile.AbsPath});
            });
    }

    void EditorService::Initialize()
    {
        // The one place editor code resolves from the locator (composition root, D3). Engine subsystems
        // exist now (called post Engine::Startup), so their references are valid and lifetime-stable.
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        // Resolved before anything reads it: ResolveLayoutIniPath below, then the EditorContext.
        CacheEditorPaths();

        // --- ImGui context -----------------------------
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // A panel moves by its TITLE BAR, never by its body. ImGui's default lets a drag on a
        // FLOATING window's background move the window, and ImGui::Image is not an interactive item
        // — so a marquee drawn on an undocked Viewport dragged the panel instead of selecting.
        // Docked panels were unaffected, which is exactly what made it look like a viewport bug.
        //
        // Global rather than a per-panel flag: it is the convention every editor already follows,
        // and dragging inside the Hierarchy's body should not move that panel either. Drag-drop is
        // untouched — a payload source is an ITEM, and items outrank a window move regardless.
        lIO.ConfigWindowsMoveFromTitleBarOnly = true;
        //ImGui::StyleColorsDark();
        ImGui::StyleColorsClassic();
        //ImGui::StyleColorsLight();


        // --- Dock layout persistence. Set BEFORE the first NewFrame: that is where ImGui loads the ini
        //     (it only ever loads once, on the frame it first sees a filename). Empty => keep M0's
        //     null/no-persistence behaviour rather than writing a stray file next to the exe. ----------
        m_LayoutIniPath = ResolveLayoutIniPath();
        lIO.IniFilename = m_LayoutIniPath.IsEmpty() ? nullptr : m_LayoutIniPath.CStr();

        if (!m_LayoutIniPath.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Info, "Dock layout: {}", m_LayoutIniPath.CStr());
        }

        // --- UI backend (OpenGL today, S7). The window was created in InitializeApplication and its GL
        //     context is current on this thread, so ImGui_ImplOpenGL3_Init is safe here. Built BEFORE the
        //     EditorContext so the context can carry a reference to it (the ViewportPanel samples its FBO
        //     through it — GetViewportImage). --------------------------------------------------------
        IWindowManager& lWindows = OpaaxApplication::GetAppService<IWindowManager>();
        Window* lWindow = lWindows.GetMainWindow();
        if (lWindow == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "No main window at editor init — ImGui UI backend not created.");
            return;
        }

        m_UIBackend = MakeUnique<OpenGLEditorUIBackend>(static_cast<GLFWwindow*>(lWindow->GetNativeWindow()));
        m_UIBackend->Init();

        // --- Selection (M2a): the single selected entity, owned here so the context can hold a
        //     reference to it. Nothing reads it yet — S2's Hierarchy panel is the first writer. -------
        m_Selection = MakeUnique<EditorSelection>();

        // --- ②: the viewport's pixel size. Owned here, not by the panel that measures it, so a
        //     command outside the panel can ask for the aspect (focus-selected). ------------------
        m_Viewport = MakeUnique<EditorViewport>();

        // --- ④b: what a double-click asked to preview. Owned here for the same reason the selection
        //     is — a resource type's activate closure writes it, the Preview panel reads it. --------
        m_Preview = MakeUnique<ResourcePreview>();

        // --- ①: how the author is looking at an Edit world. Owned HERE and not by the ViewportPanel,
        //     because it has to survive a PIE cycle — Play swaps the active world, this object does
        //     not move, and Stop finds the pan and zoom exactly where they were left. ---------------
        m_Camera = MakeUnique<EditorCamera>();

        // --- ③: the transform handles. Owned here for the reason the selection is — its subject is
        //     the selection, and no panel owns that (SEL7). ------------------------------------------
        m_Gizmo = MakeUnique<EditorGizmo>();

        // --- PIE (M4 S5): the Play/Pause/Step/Stop state machine, owned here so BOTH front-ends —
        //     the toolbar panel and RouteInput's reserved keys — drive one object. -----------------
        m_PIE = MakeUnique<PlayInEditor>(lEngine.GetWorldManager());

        // --- The input route (M-Input S2): D5's steps 2 and 4 in one place, so RouteInput's
        //     behaviour and the Input panel's readout can never disagree. Built after PIE — it
        //     reads the play state, since a paused session must not be fed. -------------------
        m_InputRoute = MakeUnique<InputRoute>(lEngine.GetWorldManager(), lEngine.GetInput(), *m_PIE);

        // --- The open map (M5 S5). Created empty; it ADOPTS the world the engine has already built
        //     from the project's startup level a few lines below, once the context exists. -------
        m_MapDocument = MakeUnique<EditorMapDocument>();
        m_LevelDocument = MakeUnique<EditorLevelDocument>();

        // --- The live panels. Created EMPTY here so the context below can hold a reference to it;
        //     Build() runs the factories once the context exists. ------------------------------
        m_PanelHost = MakeUnique<EditorPanels>();

        // --- World-switch reactions (M4 S5). PIE swaps the active world twice per session, so a
        //     cached selection or per-world panel state has to be told. Subscribed BEFORE the panels
        //     exist: the first switch cannot happen until a frame runs, and unsubscribing is
        //     OnShutdown's job (which runs while the engine is still alive). --------------------
        m_SubscribedWorlds = &lEngine.GetWorldManager();
        m_SubscribedWorlds->OnActiveWorldChanged.AddMember(this, &EditorService::HandleActiveWorldChanged);
        m_SubscribedWorlds->OnWorldDestroyed.AddMember(this, &EditorService::HandleWorldDestroyed);

        // --- EditorContext: the flat ref bundle every panel/drawer receives by ctor (D3). Built after
        //     the UIBackend so it can hold a reference to it. ---------------------------------------
        m_Context = MakeUnique<EditorContext>(EditorContext{
            lEngine,
            lEngine.GetWorldManager(),
            lEngine.GetResources(),
            *m_UIBackend,
            *m_Selection,
            *m_Viewport,
            *m_Camera,
            *m_Gizmo,
            *m_PIE,
            *m_InputRoute,
            *m_LevelDocument,
            *m_MapDocument,
            m_Extensions,
            *m_PanelHost,
            *m_Preview,
            OpaaxApplication::GetAppService<IPaths>(),
            OpaaxApplication::GetAppService<IPlatform>().GetFileSystem(),
            OpaaxApplication::GetAppService<IConfigSystem>(),
            *lWindow,
            m_EditorPaths
        });

        // --- Registered panels (M2a): native and game panels alike are built HERE, from the one registry,
        //     in registration order. The factories were stored back at RegisterExtensions (pre-Engine
        //     startup, no context yet) — this is the point where they finally have one to receive.
        //     The Viewport is simply the first of them: its Startup registers the offscreen FBO as the
        //     engine's primary render target, so the world renders into a panel's texture. ------------
        m_PanelHost->Build(m_Extensions.Panels(), *m_Context);

        OPAAX_LOG(LogEditorService, Info, "Editor panels registered: {}, constructed: {}",
                  m_Extensions.Panels().Count(), m_PanelHost->Count());

        AdoptStartupLevel();

        OPAAX_LOG(LogEditorService, Info, "EditorService initialized (EditorContext bound, ImGui docking UI up)");
    }

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

    void EditorService::BeginFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        // Re-decide the input route ONCE per frame, here rather than inside RouteInput: a rule
        // evaluated only when an event arrives cannot notice that input STOPPED — and "the route
        // just closed" is precisely the case that has to reset the engine's held keys. Reads the
        // viewport hover/focus the panel pushed last frame, BEFORE OnPreRender clears it.
        if (m_InputRoute != nullptr) { m_InputRoute->Evaluate(); }

        m_UIBackend->NewFrame();
        ImGui::NewFrame();

        // ③ — right after ImGui's own NewFrame, as ImGuizmo's header asks. Needed even though the
        // ViewportPanel calls SetDrawlist: this is what clears the per-frame hotspot flags IsOver()
        // reads, and a stale one would leave the marquee suppressed after the cursor left a handle.
        ImGuizmo::BeginFrame();

        // Apply any pending viewport resize (measured last DrawContents) BEFORE Engine().Loop()
        // renders the world, so Render() reads the new FBO size this frame (deferred-resize
        // handshake, §5).
        if (m_PanelHost != nullptr) { m_PanelHost->OnPreRender(); }
    }

    void EditorService::EndFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        // Once per frame, ahead of everything that reads it — the Hierarchy draws a `*` per map.
        RefreshDirtyCache();

        DrawDockspace();

        // The Viewport panel samples the FBO the world was just rendered into (Engine().Loop() above)
        // and shows it as an ImGui image — the world lives INSIDE a panel now, not the raw backbuffer.
        // It is the first panel in the host, with no special path of its own.
        if (m_PanelHost != nullptr) { m_PanelHost->Draw(); }

        // Submit the UI to the backbuffer AFTER Engine().Loop() has rendered the world into the FBO
        // (see EditorApplication::TickFrame). The host presents the backbuffer once, after this.
        ImGui::Render();
        m_UIBackend->RenderDrawData();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            m_UIBackend->RenderPlatformWindows();
        }
    }

    bool EditorService::RouteInput(Event& InEvent)
    {
        // D5's decision order, steps 1 and 3. Step 2 (viewport hover/focus) and step 4 (dispatch by
        // world mode, InputManager feed + ResetState) are M-Input — NOT here.
        if (m_UIBackend == nullptr) { return false; } // UI not up (pre-Initialize / no window) — pass through

        const ImGuiIO& lIO = ImGui::GetIO();

        // The viewport is an ImGui window like any other — an image with the world drawn into it —
        // so ImGui reports WantCaptureMouse the whole time the pointer is over it. Taken at face
        // value that means a game running inside the editor can NEVER receive a click, a drag or
        // the wheel, which is not what step 1 is for: ImGui owns the pointer over the UI, and the
        // game owns it over the surface it is being played on.
        //
        // Keyboard is NOT exempted. WantCaptureKeyboard only goes true for a text field, and a
        // field that has the keyboard must always win, viewport or not.
        const bool lViewportHovered = m_InputRoute != nullptr && m_InputRoute->IsViewportHovered();

        bool lConsumed = false;
        if (InEvent.IsInCategory(EEventCategory::Mouse) || InEvent.IsInCategory(EEventCategory::MouseButton))
        {
            lConsumed = lIO.WantCaptureMouse && !lViewportHovered;
        }
        else if (InEvent.IsInCategory(EEventCategory::Keyboard))
        {
            lConsumed = lIO.WantCaptureKeyboard;
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
        // Over the ImGui UI -> WantCapture true -> CONSUMED; over the passthru viewport -> passed to engine.
        if (InEvent.IsInCategory(EEventCategory::MouseButton) || InEvent.IsInCategory(EEventCategory::Keyboard))
        {
            OPAAX_LOG(LogEditorService, Trace, "RouteInput: {} -> {} (WantMouse={}, WantKeyboard={})",
                      InEvent.GetName(), lConsumed ? "CONSUMED by editor" : "passed to engine",
                      lIO.WantCaptureMouse, lIO.WantCaptureKeyboard);
        }

        return lConsumed;
    }

    void EditorService::RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect)
    {
        RegisterNativePanels();
        RegisterNativeMenus();
        RegisterNativeResourceTypes();
        RegisterNativeEditorCommand();
        RegisterNativeConfigDrawers();

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

        // Dispatched BY TAG, exactly as the Play menu does it — a key and a menu entry must reach
        // one verb, not two copies of it. This is also the table a shortcut system would own: the
        // key-to-tag mapping is the only part that would move out of here.
        //
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

        if (m_PanelHost != nullptr)
        {
            m_PanelHost->OnActiveWorldChanged(InOld, InNew);
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

    void EditorService::CacheEditorPaths()
    {
        // EditorSaveDir/EditorAssetsDir live only on EditorPaths, deliberately: the engine's IPaths knows
        // nothing about an editor (D4). EditorApplication::CreatePaths normally installs EditorPaths, but it
        // falls back to a plain Paths when no edited project is declared — so this cast genuinely can fail.
        // Done ONCE here: the dock layout and the Resource Browser both need it.
        const IPaths& lPaths = OpaaxApplication::GetAppService<IPaths>();
        m_EditorPaths = dynamic_cast<const EditorPaths*>(&lPaths);

        if (m_EditorPaths == nullptr)
        {
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths (no edited project?) — editor space unavailable.");
        }
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

    void EditorService::RegisterNativePanels()
    {
        PanelRegistry& lPanels = m_Extensions.Panels();
        
        lPanels.Register<ViewportPanel>(PanelDesc{.Id = OPAAX_ID("Viewport")});
        lPanels.Register<PlayToolbarPanel>(PanelDesc{.Id = OPAAX_ID("Play Controls")});
        lPanels.Register<HierarchyPanel>(PanelDesc{.Id = OPAAX_ID("Hierarchy")});
        lPanels.Register<InspectorPanel>(PanelDesc{.Id = OPAAX_ID("Inspector")});
        lPanels.Register<ResourceBrowserPanel>(PanelDesc{.Id = OPAAX_ID("Resource Browser")});

        // Hidden until something is double-clicked — an empty preview is not worth a pane on a
        // fresh layout, and the activate that fills it is also what shows it.
        lPanels.Register<ResourcePreviewPanel>(PanelDesc{.Id = PreviewPanelId(),
                                                         .DefaultVisibility = EPanelVisibility::Hidden});
        lPanels.Register<ConfigPanel>(PanelDesc{.Id = OPAAX_ID("Config"), .DefaultVisibility = EPanelVisibility::Hidden});
        lPanels.Register<InputPanel>(PanelDesc{.Id = OPAAX_ID("Input"), .DefaultVisibility = EPanelVisibility::Hidden });
    }

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

    void EditorService::DrawDockspace()
    {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (m_Context != nullptr)
        {
            m_Extensions.Menus().Draw(*m_Context);
        }

        HandleAuthoringShortcuts();
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

        const double lNow = ImGui::GetTime();
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

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_MAP, *m_Context);
        }

        // F and Delete are EDITOR-WIDE, not the viewport's. They were measured on the viewport
        // first, which meant they did nothing from the Hierarchy — the panel an author is most
        // likely to be in when deleting something. Their subject is the SELECTION, and the
        // selection is not owned by any one panel, so neither are its verbs.
        //
        // What made a bare key unsafe was never the route, it was a text field: guarding on
        // WantCaptureKeyboard is what lets these be global, so typing "Fred" into the name field
        // cannot frame and delete the selection.
        if (ImGui::GetIO().WantCaptureKeyboard) { return; }

        if (ImGui::Shortcut(ImGuiKey_F, ImGuiInputFlags_RouteGlobal))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_FOCUS_SELECTED, *m_Context);
        }

        if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal))
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

        if (ImGui::Shortcut(ImGuiKey_W, ImGuiInputFlags_RouteGlobal))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_TRANSLATE, *m_Context);
        }

        if (ImGui::Shortcut(ImGuiKey_E, ImGuiInputFlags_RouteGlobal))
        {
            m_Context->Extensions.Commands().Execute(Tags::EDITOR_COMMAND_GIZMO_ROTATE, *m_Context);
        }

        if (ImGui::Shortcut(ImGuiKey_R, ImGuiInputFlags_RouteGlobal))
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
        if (m_SubscribedWorlds != nullptr)
        {
            m_SubscribedWorlds->OnActiveWorldChanged.RemoveAll(this);
            m_SubscribedWorlds->OnWorldDestroyed.RemoveAll(this);
            m_SubscribedWorlds = nullptr;
        }

        // 1. Every panel, reverse construction order (LC3). The Viewport registered first so it dies
        //    LAST, which is the right end: its Shutdown clears the engine's primary render target
        //    while the engine is alive (no live frame reads a dangling target) and frees the FBO
        //    while the GL context is still current — both true here, since the UI backend below has
        //    not gone yet. All of it must precede m_Context.reset(): panels hold a reference into it.
        if (m_PanelHost != nullptr)
        {
            m_PanelHost->Shutdown();
        }

        // 3. UI backend — ImGui_ImplOpenGL3_Shutdown requires the GL context, still alive here.
        //    DestroyContext also FLUSHES the dock layout, through the io.IniFilename pointer that still
        //    aims at m_LayoutIniPath — so that member must not be cleared before this line.
        if (m_UIBackend != nullptr)
        {
            m_UIBackend->Shutdown();
            ImGui::DestroyContext();
            m_UIBackend.reset();
        }

        // 4. Selection, PIE and the input route — after the panels that read them, before the
        //    context they are referenced from. All three hold only non-owning references, so there
        //    is nothing to undo; the route is dropped before the engine it would reset.
        m_Selection.reset();
        m_Viewport.reset();
        m_Preview.reset();
        m_Camera.reset();
        m_Gizmo.reset();
        m_InputRoute.reset();
        m_PIE.reset();
        m_PanelHost.reset();

        // 5. The context refs last (nothing points into them anymore).
        m_Context.reset();
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
