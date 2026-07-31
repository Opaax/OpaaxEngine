#include "Editor/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/EditorPaths.h"                            // EditorSaveDir — the dock layout's home (D4)

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"                    // OPAAX_LOG + LogCategory
#include "Application/Services/Platforms/IPlatform.h"        // GetFileSystem — the dock-layout dir
#include "Application/Services/Platforms/IFileSystem.h"
#include "Application/Services/Window/IWindowManager.h"      // window + native GLFW handle
#include "Core/Events/Event.h"                               // Event::IsInCategory + EEventCategory (S11)
#include "Engine/Registries/EngineRegistries.h"              // EditWorldSystems() binds to WorldSubsystems()
#include "Engine/Subsystems/Input/InputEvents.h"             // KeyPressedEvent — the reserved keys (D5 step 3)
#include "World/Entity/Entity.h"                             // selection retarget by Guid across a world switch
#include "World/World.h"
#include "World/WorldManager.h"

#include <imgui.h>

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorService{"EditorService"};
}

namespace Opaax::Editor
{
    void EditorService::Initialize()
    {
        // The one place editor code resolves from the locator (composition root, D3). Engine subsystems
        // exist now (called post Engine::Startup), so their references are valid and lifetime-stable.
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        // Resolved before anything reads it: ResolveLayoutIniPath below, then the EditorContext.
        CacheEditorPaths();

        // --- ImGui context (docking; multi-viewport deferred past M0) -----------------------------
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

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
        Window*         lWindow  = lWindows.GetMainWindow();
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

        // --- PIE (M4 S5): the Play/Pause/Step/Stop state machine, owned here so BOTH front-ends —
        //     the toolbar panel and RouteInput's reserved keys — drive one object. -----------------
        m_PIE = MakeUnique<PlayInEditor>(lEngine.GetWorldManager());

        // --- The input route (M-Input S2): D5's steps 2 and 4 in one place, so RouteInput's
        //     behaviour and the Input panel's readout can never disagree. Built after PIE — it
        //     reads the play state, since a paused session must not be fed. -------------------
        m_InputRoute = MakeUnique<InputRoute>(lEngine.GetWorldManager(), lEngine.GetInput(), *m_PIE);

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
            *m_PIE,
            *m_InputRoute,
            m_Extensions,
            OpaaxApplication::GetAppService<IPaths>(),
            OpaaxApplication::GetAppService<IPlatform>().GetFileSystem(),
            m_EditorPaths
        });

        // --- Viewport panel (M1): owns the offscreen FBO and registers it as the engine's primary
        //     render target — the world now renders into the panel's texture, not the backbuffer. ----
        m_ViewportPanel = MakeUnique<ViewportPanel>(*m_Context);
        m_ViewportPanel->Startup();

        // --- Registered panels (M2a): native and game panels alike are built HERE, from the one registry,
        //     in registration order. The factories were stored back at RegisterExtensions (pre-Engine
        //     startup, no context yet) — this is the point where they finally have one to receive. -------
        for (const PanelEntry& lEntry : m_Extensions.Panels().Entries())
        {
            UniquePtr<IEditorPanel> lPanel = lEntry.Factory ? lEntry.Factory(*m_Context) : nullptr;
            if (lPanel == nullptr)
            {
                OPAAX_LOG(LogEditorService, Warn, "Panel '{}' produced no instance — skipped.", lEntry.Id);
                continue;
            }

            lPanel->Startup();
            m_Panels.push_back(Move(lPanel));
        }

        OPAAX_LOG(LogEditorService, Info, "Editor panels registered: {}, constructed: {}",
            m_Extensions.Panels().Count(), m_Panels.size());

        OPAAX_LOG(LogEditorService, Info, "EditorService initialized (EditorContext bound, ImGui docking UI up)");
    }

    void EditorService::BeginFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        // Re-decide the input route ONCE per frame, here rather than inside RouteInput: a rule
        // evaluated only when an event arrives cannot notice that input STOPPED — and "the route
        // just closed" is precisely the case that has to reset the engine's held keys.
        if (m_InputRoute != nullptr)
        {
            const bool lHovered = m_ViewportPanel != nullptr && m_ViewportPanel->IsHovered();
            const bool lFocused = m_ViewportPanel != nullptr && m_ViewportPanel->IsFocused();

            m_InputRoute->Evaluate(lHovered, lFocused);
        }

        m_UIBackend->NewFrame();
        ImGui::NewFrame();

        // Apply any pending viewport resize (measured last Draw) BEFORE Engine().Loop() renders the
        // world, so Render() reads the new FBO size this frame (deferred-resize handshake, §5).
        if (m_ViewportPanel != nullptr) { m_ViewportPanel->OnPreRender(); }

        for (const UniquePtr<IEditorPanel>& lPanel : m_Panels) { lPanel->OnPreRender(); }
    }

    void EditorService::EndFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        DrawDockspace();

        // The Viewport panel samples the FBO the world was just rendered into (Engine().Loop() above)
        // and shows it as an ImGui image — the world lives INSIDE a panel now, not the raw backbuffer.
        if (m_ViewportPanel != nullptr) { m_ViewportPanel->Draw(); }

        for (const UniquePtr<IEditorPanel>& lPanel : m_Panels) { lPanel->Draw(); }

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
        if (m_UIBackend == nullptr) { return false; }   // UI not up (pre-Initialize / no window) — pass through

        const ImGuiIO& lIO = ImGui::GetIO();

        bool lConsumed = false;
        if (InEvent.IsInCategory(EEventCategory::Mouse) || InEvent.IsInCategory(EEventCategory::MouseButton))
        {
            lConsumed = lIO.WantCaptureMouse;
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
        // D10/§2: fired BEFORE the first world. Native editor panels register FIRST, then the game's editor
        // module(s) plug into the routes, then we seal — no more registration once the first world exists.
        RegisterNativePanels();

        // EditWorldSystems() -> the ENGINE's WorldSubsystemRegistry, the same one the game module
        // registers into (M4 S5). Bound here because this runs at OnModulesRegistered: the engine has
        // started, the registries are live, and nothing has sealed them yet. An editor Edit-world
        // candidate and a game Play-world candidate end up in one list, and each World takes the subset
        // its mode qualifies for — the editor gets no privileged path.
        m_Extensions.EditWorldSystems().Bind(&OpaaxApplication::GetAppService<IEngine>().GetRegistries().WorldSubsystems());

        if (InCollect)
        {
            InCollect(m_Extensions);
        }
        m_Extensions.Seal();

        OPAAX_LOG(LogEditorService, Info,
            "Editor extensions sealed (before first world): drawers={}, panels={}, resourceTypes={}, menus={}, editWorldSystems={}",
            m_Extensions.Drawers().Count(),  m_Extensions.Panels().Count(), m_Extensions.ResourceTypes().Count(),
            m_Extensions.Menus().Count(),    m_Extensions.EditWorldSystems().Count());
    }

    bool EditorService::HandleReservedKeys(Event& InEvent)
    {
        if (m_PIE == nullptr || InEvent.GetEventType() != KeyPressedEvent::GetStaticType())
        {
            return false;
        }

        const KeyPressedEvent& lKey = static_cast<const KeyPressedEvent&>(InEvent);
        if (lKey.IsRepeat())
        {
            // Holding F7 must not stream steps; every PIE verb is a discrete command.
            return false;
        }

        // Bare function keys, not chords: the KeyPressed payload carries no modifier state, so
        // Ctrl+P-style shortcuts are not expressible today. M-Input owns that.
        switch (lKey.GetKeyCode())
        {
        case EKeyCode::F5: m_PIE->Play();        return true;
        case EKeyCode::F6: m_PIE->TogglePause(); return true;
        case EKeyCode::F7: m_PIE->Step();        return true;
        case EKeyCode::F8: m_PIE->Stop();        return true;
        default:                                 return false;
        }
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
            const Entity lPrevious = m_Selection->Get();
            const Guid   lGuid     = lPrevious.GetGuid();

            Entity lRetargeted = InNew != nullptr ? InNew->FindByGuid(lGuid) : Entity{};

            if (lRetargeted.IsValid())
            {
                m_Selection->Select(lRetargeted);
            }
            else
            {
                // No counterpart — destroyed during play, or there is no world at all. Clearing is the
                // only correct answer: Entity holds a raw World*, so keeping it would dangle the moment
                // the old world dies (EditorSelection's M4 FIXME).
                m_Selection->Clear();
            }
        }

        for (const UniquePtr<IEditorPanel>& lPanel : m_Panels)
        {
            lPanel->OnActiveWorldChanged(InOld, InNew);
        }

        if (m_ViewportPanel != nullptr)
        {
            m_ViewportPanel->OnActiveWorldChanged(InOld, InNew);
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

        if (m_Selection->Get().GetWorld() == InWorld)
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
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths (no edited project?) — editor space unavailable.")
        }
    }

    OpaaxString EditorService::ResolveLayoutIniPath() const
    {
        const EditorPaths* lEditorPaths = m_EditorPaths;
        if (lEditorPaths == nullptr)
        {
            OPAAX_LOG(LogEditorService, Warn, "No EditorPaths — dock layout will not persist.")
            return {};
        }

        // ImGui does not create directories, and its save fails SILENTLY when one is missing — so the dir
        // has to exist before the first write, not on first save. GetPathIfNCreate is exactly that, and
        // logs its own failure detail.
        const OpaaxString  lSaveDir    = lEditorPaths->EditorSaveDir();
        const IFileSystem& lFileSystem = OpaaxApplication::GetAppService<IPlatform>().GetFileSystem();

        if (lFileSystem.GetPathIfNCreate(lSaveDir).IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn, "Could not create '{}' — dock layout will not persist.",
                lSaveDir.CStr())
            return {};
        }

        //TODO: Make a Imgui wrapper to init imgui stuff
        return lEditorPaths->EditorToAbsolute(OpaaxString("Save/imgui.ini"));
    }

    void EditorService::RegisterNativePanels()
    {
        // The PIE controls are a PANEL like any other — registered through the same route a game
        // panel travels, not drawn by EditorService as a privileged widget (D10).
        m_Extensions.Panels().Register("Play Controls",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<PlayToolbarPanel>(InContext); });

        m_Extensions.Panels().Register("Hierarchy",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<HierarchyPanel>(InContext); });

        m_Extensions.Panels().Register("Inspector",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<InspectorPanel>(InContext); });

        m_Extensions.Panels().Register("Resource Browser",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<ResourceBrowserPanel>(InContext); });

        m_Extensions.Panels().Register("Input",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<InputPanel>(InContext); });
    }

    void EditorService::DrawDockspace()
    {
        // Full-viewport dockspace. M0 used PassthruCentralNode so the raw world showed through a
        // transparent hole; M1 drops that — the world now lives in the Viewport panel (drawn in
        // EndFrame), so the central node is a normal opaque dock target the panel can dock into.
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("Exit");   // wired at S11/M-Input; a visible affordance for now
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Editor"))
            {
                if (ImGui::BeginMenu("Panels"))
                {
                    ImGui::EndMenu();
                }
                //ImGui::MenuItem("Exit");   // wired at S11/M-Input; a visible affordance for now
                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
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

        // 1. Panel FIRST — its Shutdown clears the engine's primary render target (while the engine is
        //    alive, so no live frame reads a dangling target) then frees the FBO (GL context current).
        //    Must precede m_Context.reset() — the panel holds a reference into the context.
        if (m_ViewportPanel != nullptr)
        {
            m_ViewportPanel->Shutdown();
            m_ViewportPanel.reset();
        }

        // 2. Registered panels — reverse construction order (LC3). None owns a GPU resource, so the only
        //    ordering constraint is that they die before the context they hold a reference into.
        while (!m_Panels.empty())
        {
            m_Panels.back()->Shutdown();
            m_Panels.pop_back();
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
        m_InputRoute.reset();
        m_PIE.reset();

        // 5. The context refs last (nothing points into them anymore).
        m_Context.reset();
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
