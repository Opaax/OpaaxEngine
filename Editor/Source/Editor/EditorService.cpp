#include "Editor/EditorService.h"

#include "Editor/UI/OpenGLEditorUIBackend.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"                 // OPAAX_LOG + LogCategory
#include "Application/Services/Window/IWindowManager.h"   // window + native GLFW handle

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

        m_Context = MakeUnique<EditorContext>(EditorContext{
            lEngine,
            lEngine.GetWorldManager(),
            lEngine.GetResources()
        });

        // --- ImGui context (docking; multi-viewport deferred past M0) -----------------------------
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        lIO.IniFilename  = nullptr;   // no imgui.ini written into the project dir (M0)
        ImGui::StyleColorsDark();

        // --- UI backend (OpenGL today, S7). The window was created in InitializeApplication and its GL
        //     context is current on this thread, so ImGui_ImplOpenGL3_Init is safe here. --------------
        IWindowManager& lWindows = OpaaxApplication::GetAppService<IWindowManager>();
        Window*         lWindow  = lWindows.GetMainWindow();
        if (lWindow == nullptr)
        {
            OPAAX_LOG(LogEditorService, Error, "No main window at editor init — ImGui UI backend not created.");
            return;
        }

        m_UIBackend = MakeUnique<OpenGLEditorUIBackend>(static_cast<GLFWwindow*>(lWindow->GetNativeWindow()));
        m_UIBackend->Init();

        OPAAX_LOG(LogEditorService, Info, "EditorService initialized (EditorContext bound, ImGui docking UI up)");
    }

    void EditorService::BeginFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        m_UIBackend->NewFrame();
        ImGui::NewFrame();
    }

    void EditorService::EndFrame()
    {
        if (m_UIBackend == nullptr) { return; }

        DrawDockspace();

        // Submit to the backbuffer AFTER Engine().Loop() has drawn the world into it (see
        // EditorApplication::TickFrame). The host presents the backbuffer once, after this.
        ImGui::Render();
        m_UIBackend->RenderDrawData();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            m_UIBackend->RenderPlatformWindows();
        }
    }

    void EditorService::DrawDockspace()
    {
        // Full-viewport dockspace; the central node is passthrough, so the world rendered by
        // Engine().Loop() shows through it. M0 has no panels yet (that is M1) — this is bare chrome
        // plus a menu bar, enough to prove the overlay draws over the 3 quads (gate 10d).
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

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
        // Reverse-order teardown: EditorService is provided last, so this runs FIRST — the window and its
        // GL context are still alive (LC), which ImGui_ImplOpenGL3_Shutdown requires. Tear the UI down
        // before releasing the context refs.
        if (m_UIBackend != nullptr)
        {
            m_UIBackend->Shutdown();
            ImGui::DestroyContext();
            m_UIBackend.reset();
        }

        m_Context.reset();
        OPAAX_LOG(LogEditorService, Info, "EditorService shutdown");
    }
}
