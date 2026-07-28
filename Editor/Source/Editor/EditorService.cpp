#include "Editor/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ResourceBrowserPanel.h"
#include "Editor/EditorPaths.h"                            // EditorSaveDir — the dock layout's home (D4)

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"                    // OPAAX_LOG + LogCategory
#include "Application/Services/Platforms/IPlatform.h"        // GetFileSystem — the dock-layout dir
#include "Application/Services/Platforms/IFileSystem.h"
#include "Application/Services/Window/IWindowManager.h"      // window + native GLFW handle
#include "Core/Events/Event.h"                               // Event::IsInCategory + EEventCategory (S11)

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

        // --- EditorContext: the flat ref bundle every panel/drawer receives by ctor (D3). Built after
        //     the UIBackend so it can hold a reference to it. ---------------------------------------
        m_Context = MakeUnique<EditorContext>(EditorContext{
            lEngine,
            lEngine.GetWorldManager(),
            lEngine.GetResources(),
            *m_UIBackend,
            *m_Selection,
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
        // S11 SEAM ONLY. The full routing policy (viewport hover/focus, reserved keys, world-mode
        // dispatch, InputManager feed + ResetState) is M-Input (Editor.md D5) — NOT here. Today the
        // route reaches exactly ImGui's capture flags: if the UI wants the pointer/keys, it eats the event.
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
        m_Extensions.Panels().Register("Hierarchy",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<HierarchyPanel>(InContext); });

        m_Extensions.Panels().Register("Inspector",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<InspectorPanel>(InContext); });

        m_Extensions.Panels().Register("Resource Browser",
            [](EditorContext& InContext) -> UniquePtr<IEditorPanel> { return MakeUnique<ResourceBrowserPanel>(InContext); });
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
            ImGui::EndMainMenuBar();
        }
    }

    void EditorService::OnShutdown()
    {
        // Reverse-order teardown: EditorService is provided last, so this runs FIRST — the engine, the
        // window and its GL context are all still alive (LC). Order within:

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

        // 4. Selection — after the panels that read/write it, before the context it points into.
        m_Selection.reset();

        // 5. The context refs last (nothing points into them anymore).
        m_Context.reset();
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
