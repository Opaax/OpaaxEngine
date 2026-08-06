#include "Editor/Application/Services/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor//Application/Services/EditorPaths.h"                            // EditorSaveDir — the dock layout's home (D4)

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"                    // OPAAX_LOG + LogCategory
#include "Application/Services/Platforms/IPlatform.h"        // GetFileSystem — the dock-layout dir
#include "Application/Services/Platforms/IFileSystem.h"
#include "Application/Services/Window/IWindowManager.h"      // window + native GLFW handle
#include "Application/Services/IProjectManager.h"            // startupLevel — which map the editor adopts (M5)
#include "Core/Events/Event.h"                               // Event::IsInCategory + EEventCategory (S11)
#include "Engine/Registries/EngineRegistries.h"              // EditWorldSystems() binds to WorldSubsystems()
#include "Engine/Subsystems/Input/InputEvents.h"             // KeyPressedEvent — the reserved keys (D5 step 3)
#include "World/Entity/Entity.h"                             // selection retarget by Guid across a world switch
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"                        // the document captures through it (M5)
#include "World/Serialization/LevelFile.h"                   // which map the startup level names (M5)

#include <imgui.h>
#include <tinyfiledialogs.h>                                 // Save As — the editor already vendors it

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

        // --- The open map (M5 S5). Created empty; it ADOPTS the world the engine has already built
        //     from the project's startup level a few lines below, once the context exists. -------
        m_MapDocument = MakeUnique<EditorMapDocument>();

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
            *m_MapDocument,
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
            TUniquePtr<IEditorPanel> lPanel = lEntry.Factory ? lEntry.Factory(*m_Context) : nullptr;
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

        AdoptStartupMap();

        OPAAX_LOG(LogEditorService, Info, "EditorService initialized (EditorContext bound, ImGui docking UI up)");
    }

    void EditorService::AdoptStartupMap()
    {
        // The engine already built this world from the project's startup level (FinishStartup).
        // The editor asks the SAME question to find out which file that was, rather than the
        // engine growing a "what did I load" accessor for one consumer — the manifest is data and
        // reading it twice is cheaper than a new piece of engine state to keep true.
        //
        // THE FIRST MAP IS THE ONE EDITED. The editor opens one map at a time (a Level composes
        // several; editing several at once is not a thing M5 offers), so "the level's first map"
        // is the rule — stated here because it is a real limitation, not an accident.
        if (m_Context == nullptr || m_MapDocument == nullptr) { return; }

        World* const lWorld = m_Context->Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        const OpaaxString lLevelRel = OpaaxApplication::GetAppService<IProjectManager>().StartupLevel();
        if (lLevelRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Info, "No startup level configured — no map is open for editing");
            return;
        }

        LevelData lLevel;
        if (!LevelFile::Load(m_Context->Paths.AssetToAbsolute(lLevelRel), lLevel) || lLevel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn, "Startup level '{}' names no map — nothing to edit",
                lLevelRel.CStr());
            return;
        }

        m_MapDocument->AdoptExisting(m_Context->Paths.AssetToAbsolute(lLevel.Maps[0]),
            *lWorld, m_Context->Engine.GetRegistries().Components());
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

        for (const TUniquePtr<IEditorPanel>& lPanel : m_Panels) { lPanel->OnPreRender(); }
    }

    void EditorService::EndFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        DrawDockspace();

        // The Viewport panel samples the FBO the world was just rendered into (Engine().Loop() above)
        // and shows it as an ImGui image — the world lives INSIDE a panel now, not the raw backbuffer.
        if (m_ViewportPanel != nullptr) { m_ViewportPanel->Draw(); }

        for (const TUniquePtr<IEditorPanel>& lPanel : m_Panels) { lPanel->Draw(); }

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

        // The viewport is an ImGui window like any other — an image with the world drawn into it —
        // so ImGui reports WantCaptureMouse the whole time the pointer is over it. Taken at face
        // value that means a game running inside the editor can NEVER receive a click, a drag or
        // the wheel, which is not what step 1 is for: ImGui owns the pointer over the UI, and the
        // game owns it over the surface it is being played on.
        //
        // Keyboard is NOT exempted. WantCaptureKeyboard only goes true for a text field, and a
        // field that has the keyboard must always win, viewport or not.
        const bool lViewportHovered = m_ViewportPanel != nullptr && m_ViewportPanel->IsHovered();

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
        // D10/§2: fired BEFORE the first world. Native editor panels register FIRST, then the game's editor
        // module(s) plug into the routes, then we seal — no more registration once the first world exists.
        RegisterNativePanels();
        RegisterNativeMenus();
        RegisterNativeResourceTypes();

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
                // the old world dies.
                m_Selection->Clear();
            }
        }

        for (const TUniquePtr<IEditorPanel>& lPanel : m_Panels)
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
        const OpaaxString  lSaveDir    = lEditorPaths->EditorSaveDir();
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
        // The PIE controls are a PANEL like any other — registered through the same route a game
        // panel travels, not drawn by EditorService as a privileged widget (D10).
        m_Extensions.Panels().Register("Play Controls",
            [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<PlayToolbarPanel>(InContext); });

        m_Extensions.Panels().Register("Hierarchy",
            [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<HierarchyPanel>(InContext); });

        m_Extensions.Panels().Register("Inspector",
            [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<InspectorPanel>(InContext); });

        m_Extensions.Panels().Register("Resource Browser",
            [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<ResourceBrowserPanel>(InContext); });

        m_Extensions.Panels().Register("Input",
            [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<InputPanel>(InContext); });
    }

    void EditorService::DrawDockspace()
    {
        // Full-viewport dockspace. M0 used PassthruCentralNode so the raw world showed through a
        // transparent hole; M1 drops that — the world now lives in the Viewport panel (drawn in
        // EndFrame), so the central node is a normal opaque dock target the panel can dock into.
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        if (ImGui::BeginMainMenuBar())
        {
            // THE WHOLE BAR comes from the registry — there is no hardcoded menu left (M5 S4).
            // The editor's own File/Exit is registered in RegisterNativeMenus exactly as a game's
            // Tools entry is, which is what makes "native features go through the same route"
            // (D10) true here rather than aspirational. It also removes the merge problem: a game
            // adding "File/Validate" lands in the same File menu, because there is only one.
            DrawMenuLevel(BuildAllIndices(), /*InDepth*/0);

            DrawDocumentStatus();

            ImGui::EndMainMenuBar();
        }

        HandleAuthoringShortcuts();
    }

    void EditorService::DrawDocumentStatus()
    {
        if (m_Context == nullptr || m_MapDocument == nullptr || !m_MapDocument->HasMap())
        {
            return;
        }

        const World* const lWorld = m_Context->Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        // THROTTLED, not per frame. The dirty check is a capture + serialize of the whole map —
        // fine for three quads, not fine for a real one — and running it every frame is exactly
        // what it must not do. Four times a second is far below what an eye can tell from
        // instant, and it bounds the cost at something that does not grow with framerate.
        //
        // The staleness this admits is up to 250ms of a `*` lingering after a save, which is not
        // a correctness problem: the FILE is already right, only the marker lags.
        constexpr double k_DirtyCheckInterval = 0.25;

        const double lNow = ImGui::GetTime();
        if (lNow - m_LastDirtyCheck >= k_DirtyCheckInterval)
        {
            m_LastDirtyCheck = lNow;
            m_CachedDirty    = m_MapDocument->IsDirty(*lWorld,
                                                      m_Context->Engine.GetRegistries().Components());
        }

        ImGui::Separator();
        ImGui::TextDisabled("%s%s", m_MapDocument->FileName().CStr(), m_CachedDirty ? " *" : "");
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
            SaveMapCommand(*m_Context);
        }
    }

    TDynArray<Uint32> EditorService::BuildAllIndices() const
    {
        const TDynArray<MenuEntry>& lEntries = m_Extensions.Menus().Entries();

        TDynArray<Uint32> lIndices;
        lIndices.reserve(lEntries.size());
        for (Uint32 lIndex = 0; lIndex < static_cast<Uint32>(lEntries.size()); ++lIndex)
        {
            lIndices.push_back(lIndex);
        }

        return lIndices;
    }

    void EditorService::DrawMenuLevel(const TDynArray<Uint32>& InIndices, Uint32 InDepth)
    {
        const TDynArray<MenuEntry>& lEntries = m_Extensions.Menus().Entries();

        // Names already emitted at THIS level, so two entries sharing a submenu produce one
        // submenu rather than two with the same label. Registration order decides which comes
        // first; everything sharing that prefix follows it in.
        TDynArray<OpaaxString> lEmitted;
        const auto lAlreadyEmitted = [&lEmitted](const OpaaxString& InName)
        {
            for (const OpaaxString& lName : lEmitted)
            {
                if (lName == InName) { return true; }
            }
            return false;
        };

        for (const Uint32 lIndex : InIndices)
        {
            const OpaaxString lSegment = MenuPathSegment(lEntries[lIndex].Path, InDepth);
            if (lSegment.IsEmpty() || lAlreadyEmitted(lSegment)) { continue; }

            // A LEAF is an entry with nothing after this segment — the label of the command.
            if (IsMenuPathLeaf(lEntries[lIndex].Path, InDepth))
            {
                if (ImGui::MenuItem(lSegment.CStr()) && m_Context != nullptr)
                {
                    lEntries[lIndex].Command(*m_Context);
                }

                lEmitted.push_back(lSegment);
                continue;
            }

            // A SUBMENU: gather everything sharing this segment at this depth and recurse. The
            // gather is what lets a flat registration list render as a tree without one being
            // stored anywhere.
            TDynArray<Uint32> lChildren;
            for (const Uint32 lOther : InIndices)
            {
                if (MenuPathSegment(lEntries[lOther].Path, InDepth) == lSegment)
                {
                    lChildren.push_back(lOther);
                }
            }

            if (ImGui::BeginMenu(lSegment.CStr()))
            {
                DrawMenuLevel(lChildren, InDepth + 1);
                ImGui::EndMenu();
            }

            lEmitted.push_back(lSegment);
        }
    }

    void EditorService::RegisterNativeMenus()
    {
        // The editor's own entries go through the SAME route a game module uses — registered
        // first, for the same reason native panels are (MR2's order, one level down).
        //
        // Exit finally does something. It was a bare MenuItem with a comment promising it would
        // be wired "at S11/M-Input" — a milestone that has since come and gone, which is what
        // makes a note like that a work item rather than a plan. It closes through
        // Window::RequestClose, so it takes the same path as clicking the X: one close path, not
        // a second one to keep correct.
        Window* const lWindow = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();

        // --- M5 S5/S6: the author loop, through the same route a game's Tools entry uses ------
        m_Extensions.Menus().Register("File/Open Map...",
            [](EditorContext& InContext) { OpenMapCommand(InContext); });

        m_Extensions.Menus().Register("File/Save Map",
            [](EditorContext& InContext) { SaveMapCommand(InContext); });

        m_Extensions.Menus().Register("File/Save Map As...",
            [](EditorContext& InContext) { SaveMapAsCommand(InContext); });

        m_Extensions.Menus().Register("File/Exit",
            [lWindow](EditorContext&)
            {
                OPAAX_LOG(LogEditorService, Info, "Exit requested from the File menu");
                if (lWindow != nullptr) { lWindow->RequestClose(); }
            });
    }

    void EditorService::RegisterNativeResourceTypes()
    {
        // `.opaaxmap` registered through the SAME ResourceTypes() route a game's `.wave` uses
        // (M2d), so a map is a file type like any other: it gets an icon, a label, and a
        // double-click that opens it. The editor's own core format gets no privileged path into
        // the browser — which is the property that keeps the route honest.
        m_Extensions.ResourceTypes().Register(ResourceTypeDesc{
            .Extension  = OPAAX_ID(".opaaxmap"),
            .Label      = OPAAX_ID("Opaax Map"),
            .Icon       = OpaaxString("[M]"),
            .OnActivate = [](EditorContext& InContext, const ResourceFile& InFile)
            {
                OpenMapAt(InContext, InFile.AbsPath);
            }
        });

        m_Extensions.ResourceTypes().Register(ResourceTypeDesc{
            .Extension  = OPAAX_ID(".opaaxlevel"),
            .Label      = OPAAX_ID("Opaax Level"),
            .Icon       = OpaaxString("[L]"),
            // No action: a level is a MANIFEST, and M5 has no level editor to open it in. An
            // icon and a label are a perfectly good registration (ResourceTypeDesc says so), and
            // it stops a .opaaxlevel showing up as an unknown file next to the map it composes.
            .OnActivate = {}
        });
    }

    // =============================================================================
    // The author loop's commands
    //
    // Free-standing statics rather than members: a menu command's whole input is the
    // EditorContext it is handed (D3), so nothing here needs EditorService — and keeping them
    // context-only is what lets the identical function serve the menu item AND the keyboard
    // shortcut without one of them becoming the "real" path.
    // =============================================================================
    bool EditorService::CanEditMap(const EditorContext& InContext)
    {
        // Only in EDIT state. While a PIE session runs (playing OR paused) the ACTIVE world is a
        // Play clone, and writing it back would persist simulation state — quads caught
        // mid-oscillation — over the authored map. The rule is about WHICH WORLD is on screen,
        // not about being cautious, which is why it reads the PIE state rather than a flag.
        return InContext.PIE.IsEdit() && InContext.Worlds.GetActiveWorld() != nullptr;
    }

    void EditorService::SaveMapCommand(EditorContext& InContext)
    {
        if (!CanEditMap(InContext))
        {
            OPAAX_LOG(LogEditorService, Warn, "Save Map ignored — stop the PIE session first");
            return;
        }

        if (!InContext.MapDocument.HasMap())
        {
            SaveMapAsCommand(InContext);   // nothing to overwrite — ask where
            return;
        }

        InContext.MapDocument.Save(*InContext.Worlds.GetActiveWorld(),
                                   InContext.Engine.GetRegistries().Components());
    }

    void EditorService::SaveMapAsCommand(EditorContext& InContext)
    {
        if (!CanEditMap(InContext))
        {
            OPAAX_LOG(LogEditorService, Warn, "Save Map As ignored — stop the PIE session first");
            return;
        }

        const char* const lFilters[] = { "*.opaaxmap" };

        const char* const lPicked = tinyfd_saveFileDialog(
            "Save Map As",
            InContext.MapDocument.HasMap() ? InContext.MapDocument.AbsPath().CStr()
                                           : InContext.Paths.AssetToAbsolute(OpaaxString("Maps/Untitled.opaaxmap")).CStr(),
            1, lFilters, "Opaax Map");

        if (lPicked == nullptr)
        {
            return;   // cancelled — not a failure, and not worth a log line
        }

        InContext.MapDocument.SaveAs(OpaaxString(lPicked), *InContext.Worlds.GetActiveWorld(),
                                     InContext.Engine.GetRegistries().Components());
    }

    void EditorService::OpenMapCommand(EditorContext& InContext)
    {
        if (!CanEditMap(InContext))
        {
            OPAAX_LOG(LogEditorService, Warn, "Open Map ignored — stop the PIE session first");
            return;
        }

        const char* const lFilters[] = { "*.opaaxmap" };

        const char* const lPicked = tinyfd_openFileDialog(
            "Open Map",
            InContext.Paths.AssetToAbsolute(OpaaxString("Maps/")).CStr(),
            1, lFilters, "Opaax Map", /*allowMultiple*/0);

        if (lPicked == nullptr)
        {
            return;   // cancelled
        }

        OpenMapAt(InContext, OpaaxString(lPicked));
    }

    void EditorService::OpenMapAt(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        if (!CanEditMap(InContext))
        {
            OPAAX_LOG(LogEditorService, Warn, "Open ignored — stop the PIE session first");
            return;
        }

        World* const lWorld = InContext.Worlds.GetActiveWorld();

        // UNSAVED WORK IS CONFIRMED, NOT DISCARDED. A modal is the right tool here precisely
        // because the action is irreversible — Open clears the world, and there is no undo to
        // fall back on. tinyfiledialogs is already the editor's file-dialog vendor, so this costs
        // no new dependency.
        if (InContext.MapDocument.IsDirty(*lWorld, InContext.Engine.GetRegistries().Components()))
        {
            const int lAnswer = tinyfd_messageBox(
                "Unsaved changes",
                "The current map has unsaved changes.\nOpen another map and lose them?",
                "yesno", "warning", /*defaultButton*/0);   // default NO — the safe answer

            if (lAnswer != 1)
            {
                OPAAX_LOG(LogEditorService, Info, "Open cancelled — unsaved changes kept");
                return;
            }
        }

        // The SELECTION is cleared before the world is: it holds an Entity, and every entity in
        // the world is about to stop existing. HandleActiveWorldChanged cannot cover this — the
        // active world is not changing, only its contents.
        InContext.Selection.Clear();

        InContext.MapDocument.Open(InAbsPath, *lWorld, InContext.Engine.GetRegistries().Components());
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
