#include "Editor/Application/Services/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InputPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/PlayToolbarPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/Operation/MapOperations.h"                   // the per-map verbs, shared with the Hierarchy
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
#include "World/Level.h"                                     // the open level: which maps are mounted (WM1a)
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"                        // the document captures through it (M5)
#include "World/Serialization/MapData.h"                     // MapData::OwnerId — is this map already in?
#include "World/Serialization/MapFile.h"                     // reading the picked map to ask which one it is

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
        m_MapDocument   = MakeUnique<EditorMapDocument>();
        m_LevelDocument = MakeUnique<EditorLevelDocument>();

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
            *m_LevelDocument,
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

        AdoptOpenLevel(*m_Context, lLevelRel.IsEmpty() ? OpaaxString()
                                                       : m_Context->Paths.AssetToAbsolute(lLevelRel));
    }

    void EditorService::AdoptOpenLevel(EditorContext& InContext, const OpaaxString& InLevelAbsPath)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lWorld == nullptr || lLevel == nullptr) { return; }

        // An EMPTY level path is not "no document": a standalone map still gets a record, which is
        // what keeps Save Map working in a world that has no manifest behind it.
        InContext.LevelDocument.AdoptExisting(InLevelAbsPath, *lLevel, *lWorld,
                                              InContext.Engine.GetRegistries().Components(),
                                              InContext.Paths);

        const TDynArray<Level::MountedMap>& lMounted = lLevel->GetMountedMaps();
        if (lMounted.empty())
        {
            InContext.MapDocument.Clear();
            OPAAX_LOG(LogEditorService, Info, "World '{}' has no map mounted — nothing to edit",
                lWorld->GetName().CStr());
            return;
        }

        // THE FIRST NON-PERSISTENT MAP IS THE ONE EDITED (WM1a). The persistent map is the shared
        // backdrop — the player, the lights — authored once precisely so it is not the thing being
        // worked on; the session opens on the content composed over it. Only the persistent one
        // mounted falls back to it, because there is nothing else to open.
        const MapId              lPersistent = lLevel->GetPersistentMapId();
        const Level::MountedMap* lEdited     = &lMounted[0];

        for (const Level::MountedMap& lCandidate : lMounted)
        {
            if (lCandidate.Id != lPersistent) { lEdited = &lCandidate; break; }
        }

        OPAAX_LOG(LogEditorService, Info, "Level '{}': {} map(s) mounted, focused on '{}'",
            lLevel->GetData().Name.CStr(), lMounted.size(), lEdited->AssetRelPath.CStr());

        InContext.MapDocument.Focus(InContext.Paths.AssetToAbsolute(lEdited->AssetRelPath));
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

        // Once per frame, ahead of everything that reads it — the Hierarchy draws a `*` per map.
        RefreshDirtyCache();

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

            ImGui::EndMainMenuBar();
        }

        HandleAuthoringShortcuts();
    }

    void EditorService::RefreshDirtyCache()
    {
        if (m_Context == nullptr || m_LevelDocument == nullptr) { return; }

        const World* const lWorld = m_Context->Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        const Level* const lLevel = lWorld->GetLevel();
        if (lLevel == nullptr) { return; }

        // THROTTLED, not per frame. The dirty check is a capture + serialize PER MOUNTED MAP —
        // fine for three quads, not fine for a real level — and running it every frame is exactly
        // what it must not do. Four times a second is far below what an eye can tell from instant,
        // and it bounds the cost at something that does not grow with framerate.
        //
        // The staleness this admits is up to 250ms of a `*` lingering after a save, which is not a
        // correctness problem: the FILE is already right, only the marker lags.
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
                    // EVERY command announces itself, from the one place they are all invoked —
                    // native and game alike, and a command added later cannot forget to. A menu
                    // click is a discrete user action, which is what an Info is for ([[L31]]), and
                    // it makes "did the click reach it?" a fact rather than an inference from a
                    // silent log.
                    OPAAX_LOG(LogEditorService, Info, "Menu: '{}'", lEntries[lIndex].Path.CStr());

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
        // NEW FIRST, and not only by convention: "Save Map As..." was flagged as weird precisely
        // because nothing led INTO it — there was no way to make a map, so Save As had no workflow
        // in front of it. This is that missing half, and registration order is draw order.
        m_Extensions.Menus().Register("File/New Map...",
            [](EditorContext& InContext) { NewMapCommand(InContext); });

        m_Extensions.Menus().Register("File/Open Map...",
            [](EditorContext& InContext) { OpenMapCommand(InContext); });

        m_Extensions.Menus().Register("File/Save Map",
            [](EditorContext& InContext) { SaveMapCommand(InContext); });

        m_Extensions.Menus().Register("File/Save Map As...",
            [](EditorContext& InContext) { SaveMapAsCommand(InContext); });

        // --- the LEVEL half (WM1a): what a session actually has open --------------------------
        m_Extensions.Menus().Register("File/Open Level...",
            [](EditorContext& InContext) { OpenLevelCommand(InContext); });

        m_Extensions.Menus().Register("File/Save Level",
            [](EditorContext& InContext) { SaveLevelCommand(InContext); });

        // The ONLY Level entry, and the only one that ever earned a place on the bar: it does not
        // need a map named first, it goes and picks one.
        //
        // "Remove Open Map" and "Set Open Map Persistent" were registered here too and are GONE.
        // Both acted on whatever map happened to be FOCUSED, which is not how an author picks one
        // map out of the several a level holds (WM1a) — and choosing the persistent map especially
        // needs somewhere to SEE the current answer, not just a verb aimed at the cursor. They live
        // on the Hierarchy's map headers now, where the map you click is the argument (MapOps).
        m_Extensions.Menus().Register("Level/Add Map...",
            [](EditorContext& InContext) { AddMapToLevelCommand(InContext); });

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
            .OnActivate = [](EditorContext& InContext, const ResourceFile& InFile)
            {
                OpenLevelAt(InContext, InFile.AbsPath);
            }
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
    void EditorService::SaveMapCommand(EditorContext& InContext)
    {
        if (!MapOps::CanEdit(InContext, "Save Map")) { return; }

        if (!InContext.MapDocument.HasMap())
        {
            SaveMapAsCommand(InContext);   // nothing to overwrite — ask where
            return;
        }

        // ONE map — the FOCUSED one, because this entry is on the File menu and the cursor is what
        // a File command has. The Hierarchy's per-map Save names its target instead (MapOps).
        MapOps::Save(InContext, InContext.MapDocument.GetMapId());
    }

    void EditorService::SaveMapAsCommand(EditorContext& InContext)
    {
        if (!MapOps::CanEdit(InContext, "Save Map As")) { return; }

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

        if (InContext.LevelDocument.SaveMapAs(InContext.MapDocument.GetMapId(), OpaaxString(lPicked),
                                              *InContext.Worlds.GetActiveWorld(),
                                              InContext.Engine.GetRegistries().Components()))
        {
            // The cursor follows the file it just wrote; the record already moved with it.
            InContext.MapDocument.Focus(OpaaxString(lPicked));
        }
    }

    void EditorService::NewMapCommand(EditorContext& InContext)
    {
        if (!MapOps::CanEdit(InContext, "New Map")) { return; }

        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        const char* const lFilters[] = { "*.opaaxmap" };

        const char* const lPicked = tinyfd_saveFileDialog(
            "New Map",
            InContext.Paths.AssetToAbsolute(OpaaxString("Maps/NewMap.opaaxmap")).CStr(),
            1, lFilters, "Opaax Map");

        if (lPicked == nullptr) { return; }   // cancelled

        const OpaaxString lAbsPath  = OpaaxString(lPicked);
        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(lAbsPath);

        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn,
                "'{}' is outside the project's Assets — a level can only name assets of this project",
                lPicked);
            return;
        }

        // NEW MEANS NEW. The OS save dialog warns about overwriting, but "New Map" truncating a map
        // that already has entities in it is not a thing to leave to a dialog the author is used to
        // clicking through. Open Map and Add Map are the verbs for a file that exists.
        if (InContext.FileSystem.IsPathExist(lAbsPath))
        {
            OPAAX_LOG(LogEditorService, Warn,
                "'{}' already exists — use Open Map or Level/Add Map... instead of overwriting it",
                lAssetRel.CStr());
            return;
        }

        // WRITTEN BEFORE IT IS MOUNTED, because AddMap loads it through the ResourceManager and
        // there has to be a file to load. Stamped with its own id (**MP10**) rather than left
        // anonymous: that is what makes a map with nothing in it an ORDINARY map from its first
        // frame — saveable, removable, and settable as persistent like any other.
        MapData lData;
        lData.Id = MapFile::StemId(lAbsPath);

        if (!MapFile::Save(lAbsPath, lData))
        {
            return;   // MapFile logged which of the reasons it was
        }

        if (!lLevel->AddMap(lAssetRel))
        {
            return;   // Level logged it — already in this level, or it would not mount
        }

        // RECONCILE, never re-adopt (**MP5**): the new map gets a record, every other map keeps the
        // baseline it had.
        InContext.LevelDocument.TrackMounted(*lLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        // BOTH HALVES LAND TOGETHER. Writing the map file and leaving the membership pending was
        // the worst of both: close the editor and the file stayed while the level forgot it.
        InContext.LevelDocument.SaveManifest(*lLevel);

        // Focused, because the only reason to make a map is to start putting things in it.
        MapOps::Focus(InContext, lAssetRel);

        OPAAX_LOG(LogEditorService, Info, "Created '{}' in level '{}'",
            lAssetRel.CStr(), lLevel->GetData().Name.CStr());
    }

    void EditorService::OpenMapCommand(EditorContext& InContext)
    {
        if (!MapOps::CanEdit(InContext, "Open Map")) { return; }

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

    bool EditorService::ConfirmDiscardingLevelEdits(EditorContext& InContext)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lWorld == nullptr || lLevel == nullptr) { return true; }

        // The WHOLE level, not the focused map: Save Level writes every map (MP9), so every map is
        // what a world-destroying action would cost.
        if (!InContext.LevelDocument.IsDirty(*lWorld, InContext.Engine.GetRegistries().Components(), *lLevel))
        {
            return true;
        }

        // UNSAVED WORK IS CONFIRMED, NOT DISCARDED. A modal is the right tool precisely because
        // the action is not undoable. tinyfiledialogs is already the editor's file-dialog vendor,
        // so this costs no new dependency.
        const int lAnswer = tinyfd_messageBox(
            "Unsaved changes",
            "This level has unsaved changes.\nContinue and lose them?",
            "yesno", "warning", /*defaultButton*/0);   // default NO — the safe answer

        if (lAnswer != 1)
        {
            OPAAX_LOG(LogEditorService, Info, "Cancelled — unsaved changes kept");
            return false;
        }

        return true;
    }

    void EditorService::OpenMapAt(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        if (!MapOps::CanEdit(InContext, "Open Map")) { return; }

        // WHICH map is this? Asked of the FILE's entities (WM2). The identity decides whether it
        // is already in the world; the path could not, arriving in one shape from a file dialog
        // and another from AssetToAbsolute.
        MapData lData;
        if (!MapFile::Load(InAbsPath, lData))
        {
            OPAAX_LOG(LogEditorService, Error, "Open Map FAILED for '{}' — nothing changed", InAbsPath.CStr());
            return;
        }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lLevel != nullptr && lLevel->IsMounted(lData.Id))
        {
            // ALREADY IN THE WORLD, so this loads nothing: every map of the open level is mounted
            // (WM1a). It moves the CURSOR — the selection survives untouched because none of the
            // entities it points at go anywhere, and NOTHING IS CONFIRMED because nothing is at
            // risk: every map keeps its own baseline (MP5), so the map being left stays as dirty
            // as it was and Save Level will still write it.
            InContext.MapDocument.Focus(InAbsPath);
            return;
        }

        OpenStandaloneMap(InContext, InAbsPath);
    }

    void EditorService::OpenStandaloneMap(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        // A map that belongs to no open level gets its OWN world with an empty Level, rather than
        // being merged into a level it is not part of. That is also what makes the question
        // "which maps are in this world?" keep one answer.
        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn,
                "'{}' is outside the project's Assets — a map has to be an asset of this project to be opened",
                InAbsPath.CStr());
            return;
        }

        if (!ConfirmDiscardingLevelEdits(InContext)) { return; }

        World* const lWorld = InContext.Engine.OpenLevel(WorldSpec{OpaaxString(), EWorldMode::Edit});
        if (lWorld == nullptr || lWorld->GetLevel() == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "Could not open a world for '{}'", lAssetRel.CStr());
            return;
        }

        lWorld->GetLevel()->Mount(lAssetRel);

        // No manifest behind this world — an empty level path is what tells the document so.
        AdoptOpenLevel(InContext, OpaaxString());
    }

    void EditorService::OpenLevelAt(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        if (!MapOps::CanEdit(InContext, "Open Level")) { return; }

        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn,
                "'{}' is outside the project's Assets — a level has to be an asset of this project",
                InAbsPath.CStr());
            return;
        }

        if (!ConfirmDiscardingLevelEdits(InContext)) { return; }

        // A whole new world: the level names which maps exist in it, so opening one is not
        // something the current world can be edited into.
        if (InContext.Engine.OpenLevel(WorldSpec{lAssetRel, EWorldMode::Edit}) == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "Could not open level '{}'", lAssetRel.CStr());
            return;
        }

        AdoptOpenLevel(InContext, InAbsPath);
    }

    void EditorService::OpenLevelCommand(EditorContext& InContext)
    {
        const char* const lFilters[] = { "*.opaaxlevel" };

        const char* const lPicked = tinyfd_openFileDialog(
            "Open Level",
            InContext.Paths.AssetToAbsolute(OpaaxString("Levels/")).CStr(),
            1, lFilters, "Opaax Level", /*allowMultiple*/0);

        if (lPicked == nullptr)
        {
            return;   // cancelled
        }

        OpenLevelAt(InContext, OpaaxString(lPicked));
    }

    // =============================================================================
    // Level authoring — the manifest is edited THROUGH the world's Level (WM1a), never through a
    // second copy here: the Level is what mounts and unmounts, so it is what knows the truth.
    // =============================================================================

    void EditorService::SaveLevelCommand(EditorContext& InContext)
    {
        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr || !InContext.LevelDocument.HasLevel())
        {
            OPAAX_LOG(LogEditorService, Warn, "Save Level ignored — no level file is open");
            return;
        }

        InContext.LevelDocument.SaveAll(*InContext.Worlds.GetActiveWorld(),
                                        InContext.Engine.GetRegistries().Components(), *lLevel);
    }

    void EditorService::AddMapToLevelCommand(EditorContext& InContext)
    {
        if (!MapOps::CanEdit(InContext, "Add Map")) { return; }

        Level* const lLevel = MapOps::ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        const char* const lFilters[] = { "*.opaaxmap" };

        const char* const lPicked = tinyfd_openFileDialog(
            "Add Map to Level",
            InContext.Paths.AssetToAbsolute(OpaaxString("Maps/")).CStr(),
            1, lFilters, "Opaax Map", /*allowMultiple*/0);

        if (lPicked == nullptr) { return; }

        const OpaaxString lAssetRel = InContext.Paths.AbsoluteToAsset(OpaaxString(lPicked));
        if (lAssetRel.IsEmpty())
        {
            OPAAX_LOG(LogEditorService, Warn,
                "'{}' is outside the project's Assets — a manifest can only name assets of this project",
                lPicked);
            return;
        }

        // Mounts immediately: every map of the level is in the world (WM1a), so one that was just
        // added is no exception.
        if (lLevel->AddMap(lAssetRel))
        {
            // RECONCILE, never re-adopt: a fresh AdoptExisting would re-take every baseline from
            // the world and quietly declare every other map's unsaved edits to be the clean state.
            InContext.LevelDocument.TrackMounted(*lLevel, *InContext.Worlds.GetActiveWorld(),
                                                 InContext.Engine.GetRegistries().Components(),
                                                 InContext.Paths);

            // Structure goes to disk as it changes (EditorLevelDocument::SaveManifest).
            InContext.LevelDocument.SaveManifest(*lLevel);
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
