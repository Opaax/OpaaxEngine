#include "Editor/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"                 // OPAAX_LOG + LogCategory
#include "Application/Services/Window/IWindowManager.h"   // window + native GLFW handle
#include "Core/Events/Event.h"                             // Event::IsInCategory + EEventCategory (S11)

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

        // --- ImGui context (docking; multi-viewport deferred past M0) -----------------------------
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        lIO.IniFilename  = nullptr;   // no imgui.ini written into the project dir (M0)
        ImGui::StyleColorsDark();

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

        // --- EditorContext: the flat ref bundle every panel/drawer receives by ctor (D3). Built after
        //     the UIBackend so it can hold a reference to it. ---------------------------------------
        m_Context = MakeUnique<EditorContext>(EditorContext{
            lEngine,
            lEngine.GetWorldManager(),
            lEngine.GetResources(),
            *m_UIBackend
        });

        // --- Viewport panel (M1): owns the offscreen FBO and registers it as the engine's primary
        //     render target — the world now renders into the panel's texture, not the backbuffer. ----
        m_ViewportPanel = MakeUnique<ViewportPanel>(*m_Context);
        m_ViewportPanel->Startup();

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
    }

    void EditorService::EndFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        DrawDockspace();

        // The Viewport panel samples the FBO the world was just rendered into (Engine().Loop() above)
        // and shows it as an ImGui image — the world lives INSIDE a panel now, not the raw backbuffer.
        if (m_ViewportPanel != nullptr) { m_ViewportPanel->Draw(); }

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
        // D10/§2: fired AFTER the game module, BEFORE the first world. (Native editor modules would register
        // FIRST here — M2+ dogfooding.) The game's editor module(s) plug into the routes, then we seal —
        // no more registration once the first world exists. M0 records counts only.
        if (InCollect)
        {
            InCollect(m_Extensions);
        }
        m_Extensions.Seal();

        OPAAX_LOG(LogEditorService, Info,
            "Editor extensions sealed (before first world): drawers={}, panels={}, assetTypes={}, menus={}, editWorldSystems={}",
            m_Extensions.Drawers().Count(),  m_Extensions.Panels().Count(), m_Extensions.AssetTypes().Count(),
            m_Extensions.Menus().Count(),    m_Extensions.EditWorldSystems().Count());
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

        // 2. UI backend — ImGui_ImplOpenGL3_Shutdown requires the GL context, still alive here.
        if (m_UIBackend != nullptr)
        {
            m_UIBackend->Shutdown();
            ImGui::DestroyContext();
            m_UIBackend.reset();
        }

        // 3. The context refs last (nothing points into them anymore).
        m_Context.reset();
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
