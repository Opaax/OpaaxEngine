#include "EditorSubsystem.h"

#include "GLFW/glfw3.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Scene/SceneManager.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    bool EditorSubsystem::Startup()
    {
        OPAAX_CORE_INFO("EditorSubsystem::Startup()");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        lIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGuiStyle& lStyle = ImGui::GetStyle();
        if (lIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            lStyle.WindowRounding              = 0.f;
            lStyle.Colors[ImGuiCol_WindowBg].w = 1.f;
        }

        GLFWwindow* lNativeWindow = static_cast<GLFWwindow*>(
            GetEngineApp()->GetWindow().GetNativeWindow());

        ImGui_ImplGlfw_InitForOpenGL(lNativeWindow, true);
        ImGui_ImplOpenGL3_Init("#version 450");

        // Viewport FBO
        m_ViewportPanel.Startup();

        OPAAX_CORE_INFO("EditorSubsystem: ready.");
        return true;
    }

    void EditorSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("EditorSubsystem::Shutdown()");

        m_ViewportPanel.Shutdown();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void EditorSubsystem::Render(double /*Alpha*/)
    {
        BeginFrame();
        DrawPanels();
        EndFrame();
    }

    void EditorSubsystem::BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* lViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(lViewport->Pos);
        ImGui::SetNextWindowSize(lViewport->Size);
        ImGui::SetNextWindowViewport(lViewport->ID);

        constexpr ImGuiWindowFlags lDockFlags =
            ImGuiWindowFlags_NoDocking             |
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus            |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::Begin("##DockSpace", nullptr, lDockFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("MainDockSpace"),
            ImVec2(0.f, 0.f),
            ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::End();
    }

    void EditorSubsystem::DrawPanels()
    {
        auto* lSceneMgr = GetEngineApp()
            ? GetEngineApp()->GetSubsystem<SceneManager>()
            : nullptr;

        World* lWorld = (lSceneMgr && lSceneMgr->GetActiveScene())
            ? &lSceneMgr->GetActiveScene()->GetWorld()
            : nullptr;

        // Toolbar
        m_PlayStopPanel.Draw(lSceneMgr);

        // Hierarchy — updates selected entity
        m_HierarchyPanel.Draw(lSceneMgr);

        // Inspector — reads selected entity from hierarchy
        m_InspectorPanel.Draw(lWorld, m_HierarchyPanel.GetSelectedEntity());

        // Viewport
        m_ViewportPanel.Draw();

        // Asset Browser
        m_AssetBrowserPanel.Draw();
    }

    void EditorSubsystem::EndFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        const ImGuiIO& lIO = ImGui::GetIO();
        if (lIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* lCurrentContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(lCurrentContext);
        }
    }

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR