#include "EditorSubsystem.h"
#include "GLFW/glfw3.h"

#if OPAAX_WITH_EDITOR

#include <filesystem>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Core/CoreEngineApp.h"
#include "Core/OpaaxPath.h"
#include "Core/Window.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"
#include "Core/Log/OpaaxLog.h"
#include "Renderer/Camera2D.h"
#include "RHI/RenderCommand.h"

#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/Types/Texture2DTypeActions.h"
#include "Editor/Inspector/ComponentDrawerRegistry.h"
#include "Editor/Inspector/Drawers/TagDrawer.h"
#include "Editor/Inspector/Drawers/TransformDrawer.h"
#include "Editor/Inspector/Drawers/SpriteDrawer.h"

namespace Opaax
{
    // =============================================================================
    // PIE — temp snapshot file used for Play→Stop scene restore
    // =============================================================================
    static constexpr const char* PIE_TEMP_SCENE_PATH = "EditorTemp/PlaySession.json";

    // =============================================================================
    // Startup
    // =============================================================================
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

        m_ViewportPanel.Startup();
        RegisterViewportCallbacks();

        RegisterAssetTypeActions();
        RegisterComponentDrawers();

        m_AssetBrowserPanel.Startup();

        GetEngineApp()->SetRenderTarget(&m_ViewportPanel);

        OPAAX_CORE_INFO("EditorSubsystem: ready.");
        return true;
    }

    // =============================================================================
    // Registration
    // =============================================================================
    void EditorSubsystem::RegisterAssetTypeActions()
    {
        Editor::AssetTypeRegistry::Register(MakeUnique<Editor::Texture2DTypeActions>());

        OPAAX_CORE_TRACE("EditorSubsystem: asset type actions registered.");
    }

    void EditorSubsystem::RegisterComponentDrawers()
    {
        Editor::ComponentDrawerRegistry::Register(MakeUnique<Editor::TagDrawer>());
        Editor::ComponentDrawerRegistry::Register(MakeUnique<Editor::TransformDrawer>());
        Editor::ComponentDrawerRegistry::Register(MakeUnique<Editor::SpriteDrawer>());

        OPAAX_CORE_TRACE("EditorSubsystem: component drawers registered.");
    }

    // =============================================================================
    // Viewport callbacks
    // =============================================================================
    void EditorSubsystem::RegisterViewportCallbacks()
    {
        // NOTE: Lambda captures 'this'. EditorSubsystem outlives ViewportPanel (it owns it).
        m_ViewportPanel.OnResized = [this](Uint32 InWidth, Uint32 InHeight)
        {
            OPAAX_CORE_TRACE("EditorSubsystem: viewport resized to {}x{} — syncing camera.",
                InWidth, InHeight);

            if (auto* lCamera = GetEngineApp()->GetSubsystem<Camera2D>())
            {
                lCamera->SetViewportSize(InWidth, InHeight);
            }

            RenderCommand::SetViewport(0, 0, InWidth, InHeight);
        };
    }

    // =============================================================================
    // Shutdown
    // =============================================================================
    void EditorSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("EditorSubsystem::Shutdown()");

        Editor::AssetTypeRegistry::Clear();
        Editor::ComponentDrawerRegistry::Clear();

        m_ViewportPanel.Shutdown();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // =============================================================================
    // Render
    // =============================================================================
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

        m_MainMenuBar.Draw(*this);

        if (m_bShowHierarchy)    m_HierarchyPanel.Draw(lSceneMgr);
        if (m_bShowInspector)    m_InspectorPanel.Draw(lWorld, m_HierarchyPanel.GetSelectedEntity());
        if (m_bShowViewport)     m_ViewportPanel.Draw(m_EditorState);
        if (m_bShowAssetBrowser) m_AssetBrowserPanel.Draw();
    }

    // =============================================================================
    // PIE state machine
    // =============================================================================
    void EditorSubsystem::SetEditorState(Editor::EEditorState NewState)
    {
        if (NewState == m_EditorState) { return; }

        OPAAX_CORE_INFO("EditorSubsystem: {} -> {}",
            Editor::EditorStateToString(m_EditorState),
            Editor::EditorStateToString(NewState));

        m_EditorState = NewState;
    }

    void EditorSubsystem::EnterPlayMode()
    {
        if (m_EditorState != Editor::EEditorState::Editing)
        {
            OPAAX_CORE_WARN("EditorSubsystem::EnterPlayMode — invalid from state {}",
                Editor::EditorStateToString(m_EditorState));
            return;
        }

        SceneManager* lSceneMgr = GetEngineApp() ? GetEngineApp()->GetSceneManager() : nullptr;
        if (!lSceneMgr || !lSceneMgr->GetActiveScene())
        {
            OPAAX_CORE_WARN("EditorSubsystem::EnterPlayMode — no active scene, aborting.");
            return;
        }

        const OpaaxString lTempPath = OpaaxPath::Resolve(PIE_TEMP_SCENE_PATH);
        std::filesystem::create_directories(
            std::filesystem::path(lTempPath.CStr()).parent_path());

        SceneSerializer::Serialize(*lSceneMgr->GetActiveScene(), lTempPath.CStr());

        SetEditorState(Editor::EEditorState::Playing);
    }

    void EditorSubsystem::PauseToggle()
    {
        switch (m_EditorState)
        {
            case Editor::EEditorState::Playing:
                SetEditorState(Editor::EEditorState::Paused);
                break;
            case Editor::EEditorState::Paused:
                SetEditorState(Editor::EEditorState::Playing);
                break;
            case Editor::EEditorState::Editing:
                OPAAX_CORE_WARN("EditorSubsystem::PauseToggle — ignored in Editing state.");
                break;
        }
    }

    void EditorSubsystem::ExitPlayMode()
    {
        if (m_EditorState == Editor::EEditorState::Editing)
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — already in Editing.");
            return;
        }

        SceneManager* lSceneMgr = GetEngineApp() ? GetEngineApp()->GetSceneManager() : nullptr;
        if (!lSceneMgr || !lSceneMgr->GetActiveScene())
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — no active scene, dropping snapshot restore.");
            SetEditorState(Editor::EEditorState::Editing);
            return;
        }

        const OpaaxString lTempPath = OpaaxPath::Resolve(PIE_TEMP_SCENE_PATH);
        if (!std::filesystem::exists(lTempPath.CStr()))
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — no temp snapshot found at {}",
                lTempPath.CStr());
            SetEditorState(Editor::EEditorState::Editing);
            return;
        }

        Scene* lScene = lSceneMgr->GetActiveScene();
        lScene->GetWorld().Clear();
        SceneSerializer::Deserialize(*lScene, lTempPath.CStr());

        SetEditorState(Editor::EEditorState::Editing);
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
