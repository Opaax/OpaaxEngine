#include "EditorSubsystem.h"

#if OPAAX_WITH_EDITOR

#include <filesystem>

#include <imgui.h>

#include "Core/CoreEngineApp.h"
#include "Core/OpaaxPath.h"
#include "Core/Window.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"
#include "Core/Log/OpaaxLog.h"
#include "Renderer/Camera/CameraControllerSystem.h"
#include "Renderer/Camera/CameraSubsystem.h"
#include "Renderer/Camera/OrthographicCamera.h"
#include "RHI/RenderCommand.h"
#include "RHI/RenderAPI.h"
#include "RHI/ICommandBuffer.h"
#include "Renderer/RenderTarget.hpp"
#include "Core/Config/EngineConfig.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/Types/FontTypeActions.h"
#include "Editor/Assets/Types/SceneTypeActions.h"
#include "Editor/Assets/Types/Texture2DTypeActions.h"
#include "Editor/Events/EditorEvents.h"
#include "Editor/Inspector/Drawers/TagDrawer.h"
#include "Editor/Inspector/Drawers/TransformDrawer.h"
#include "Editor/Inspector/Drawers/SpriteDrawer.h"

#include "ECS/ComponentRegistry.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpriteComponent.h"

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

        void* lNativeWindow = GetEngineApp()->GetWindow().GetNativeWindow();

        // ImGui renderer backend selected from engine config — same backend as the renderer.
        const EBackend lBackend = RenderAPI::BackendFromString(EngineConfig::RenderBackend());
        m_UIBackend = IEditorUIBackend::Create(lBackend, lNativeWindow);
        OPAAX_CORE_ASSERT(m_UIBackend)
        m_UIBackend->Init();

        m_ViewportPanel.Startup();
        RegisterViewportCallbacks();

        RegisterAssetTypeActions();
        RegisterComponentDrawers();

        m_AssetBrowserPanel.Startup();

        // Editor event bus: panels register handlers here. Tokens stored as panel
        // members; RAII unsubscribe at panel destruction. Order is not meaningful —
        // Publish dispatches in registration order per event type.
        m_MainMenuBar.OnSubscribe(*m_EventBus);
        m_HierarchyPanel.OnSubscribe(*m_EventBus);
        m_InspectorPanel.OnSubscribe(*m_EventBus);
        m_AssetBrowserPanel.OnSubscribe(*m_EventBus);

        GetEngineApp()->SetRenderTarget(&m_ViewportPanel);

        // Editor camera owned here; CameraSubsystem only holds a non-owning observer.
        // CameraSubsystem::Startup already ran (registration order) and parked a default
        // OrthographicCamera in its owned slot — we swap the active pointer to ours.
        m_EditorCamera = MakeUnique<Editor::EditorCamera>();
        if (auto* lCameras = GetEngineApp()->GetSubsystem<CameraSubsystem>())
        {
            lCameras->SetActiveCameraNonOwning(m_EditorCamera.get());
            OPAAX_CORE_INFO("EditorSubsystem: EditorCamera installed as active (non-owning).");
        }

        OPAAX_CORE_INFO("EditorSubsystem: ready.");
        return true;
    }

    // =============================================================================
    // Registration
    // =============================================================================
    void EditorSubsystem::RegisterAssetTypeActions()
    {
        Editor::AssetTypeRegistry::Register(MakeUnique<Editor::Texture2DTypeActions>());
        Editor::AssetTypeRegistry::Register(MakeUnique<Editor::SceneTypeActions>(GetEngineApp()));
        Editor::AssetTypeRegistry::Register(MakeUnique<Editor::FontTypeActions>());

        OPAAX_CORE_TRACE("EditorSubsystem: asset type actions registered.");
    }

    void EditorSubsystem::RegisterComponentDrawers()
    {
        ComponentRegistry::RegisterDrawer<ECS::TagComponent>      (MakeUnique<Editor::TagDrawer>());
        ComponentRegistry::RegisterDrawer<ECS::TransformComponent>(MakeUnique<Editor::TransformDrawer>());
        ComponentRegistry::RegisterDrawer<ECS::SpriteComponent>   (MakeUnique<Editor::SpriteDrawer>());

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

            if (auto* lCameras = GetEngineApp()->GetSubsystem<CameraSubsystem>())
            {
                lCameras->SetViewportSize(InWidth, InHeight);
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

        // Subsystems tear down in reverse-of-registration so this runs BEFORE
        // CameraSubsystem::Shutdown. The CameraSubsystem still holds a non-owning
        // pointer to our EditorCamera — clear it explicitly so nothing inside
        // any intermediate subsystem's Shutdown can dereference a freed camera.
        if (auto* lCameras = GetEngineApp() ? GetEngineApp()->GetSubsystem<CameraSubsystem>() : nullptr)
        {
            lCameras->SetActiveCameraNonOwning(nullptr);
        }
        m_EditorCamera.reset();

        Editor::AssetTypeRegistry::Clear();
        ComponentRegistry::Clear();

        m_ViewportPanel.Shutdown();

        m_UIBackend->Shutdown();
        m_UIBackend.reset();
        ImGui::DestroyContext();
    }

    // =============================================================================
    // TickEditorCameraInput — ImGui-driven pan/zoom. MUST be called AFTER BeginFrame's
    // NewFrame ingests this frame's input events; otherwise IsMouseClicked never fires
    // and IO.MouseWheel reads stale (see feedback_imgui_input_after_newframe).
    // =============================================================================
    void EditorSubsystem::TickEditorCameraInput()
    {
        if (m_EditorState != Editor::EEditorState::Editing) { return; }
        if (!m_EditorCamera)                                { return; }

        const ImGuiIO& lIO = ImGui::GetIO();

        //------------------------------------------------------------------------------
        // Middle-mouse drag → pan. Latch only set when the press lands inside the
        // viewport panel; continues regardless of where the cursor drifts after that.
        if (!m_bMMBDragging
            && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)
            && m_ViewportPanel.IsHovered())
        {
            m_bMMBDragging         = true;
            const ImVec2 lPressPos = ImGui::GetMousePos();
            m_LastDragPos          = { lPressPos.x, lPressPos.y };
        }

        if (m_bMMBDragging)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
            {
                const ImVec2 lNow    = ImGui::GetMousePos();
                const Vector2F lCurr = { lNow.x, lNow.y };
                const Vector2F lDelta = lCurr - m_LastDragPos;
                if (lDelta.x != 0.f || lDelta.y != 0.f)
                {
                    m_EditorCamera->Pan(lDelta);
                    m_LastDragPos = lCurr;
                }
            }
            else
            {
                m_bMMBDragging = false;
            }
        }

        //------------------------------------------------------------------------------
        // Scroll wheel → zoom-at-cursor. Only fires when the cursor is over the viewport
        // panel, so scrolling over Hierarchy / Inspector / AssetBrowser does NOT zoom.
        if (lIO.MouseWheel != 0.f && m_ViewportPanel.IsHovered())
        {
            const ImVec2  lMouseAbs   = ImGui::GetMousePos();
            const Vector2F lContent   = m_ViewportPanel.GetContentScreenPos();
            const Vector2F lCursorLocal = { lMouseAbs.x - lContent.x, lMouseAbs.y - lContent.y };
            m_EditorCamera->Zoom(lIO.MouseWheel, lCursorLocal, m_ViewportPanel.GetContentSize());
        }
    }

    // =============================================================================
    // RestoreEditorCamera — invoked from every ExitPlayMode transition-to-Editing branch.
    // Detaches leftover Follow/Shake controllers so they can't tick on the editor camera
    // next frame, then re-installs the EditorCamera as the active non-owning camera.
    // =============================================================================
    void EditorSubsystem::RestoreEditorCamera()
    {
        if (!GetEngineApp()) { return; }

        if (auto* lCtrls = GetEngineApp()->GetGameSubsystem<CameraControllerSystem>())
        {
            lCtrls->DetachAll();
        }

        if (auto* lCameras = GetEngineApp()->GetSubsystem<CameraSubsystem>())
        {
            lCameras->SetActiveCameraNonOwning(m_EditorCamera.get());
            OPAAX_CORE_INFO("EditorSubsystem: editor camera restored after PIE.");
        }
    }

    // =============================================================================
    // Render
    // =============================================================================
    void EditorSubsystem::Render(double /*Alpha*/)
    {
        BeginFrame();
        // Editor camera input must run AFTER NewFrame (inside BeginFrame) so ImGui's
        // mouse / wheel state reflects this frame's input. Before DrawPanels so the
        // camera deltas apply to this frame's render of the viewport texture.
        TickEditorCameraInput();
        DrawPanels();
        EndFrame();
    }

    void EditorSubsystem::BeginFrame()
    {
        // Engine's OnRender clears the ViewportPanel FBO; ImGui draws to the default framebuffer,
        // which would otherwise accumulate stale pixels behind any moving panel (PassthruCentralNode
        // + NoBackground dockspace leak last frame's content through every gap). Clear the window
        // backbuffer through the command buffer — the world/overlay passes already restored the
        // default framebuffer (their EndRenderPass), so DefaultRenderTarget is the right target.
        const Window& lWindow = GetEngineApp()->GetWindow();
        DefaultRenderTarget lBackbuffer(lWindow.GetWidth(), lWindow.GetHeight());
        ICommandBuffer& lCmd = RenderCommand::GetCommandBuffer();
        lCmd.BeginRenderPass(lBackbuffer, ELoadOp::Clear, Vector4F(0.1f, 0.1f, 0.1f, 1.f));
        lCmd.EndRenderPass();

        m_UIBackend->NewFrame();
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
        SceneManager* lSceneMgr = GetEngineApp()
            ? GetEngineApp()->GetSubsystem<SceneManager>()
            : nullptr;
        if (!lSceneMgr) { return; }

        m_MainMenuBar.Draw(*this);

        // World is owned by CoreEngineApp — stable across MainMenuBar mutations
        // (scene transitions only swap entities, not the World container). Each
        // panel that needs the active Scene fetches it just-in-time from SceneMgr
        // (HierarchyPanel, AssetBrowserPanel) — no broker-level Scene* cache.
        World& lWorld = GetEngineApp()->GetWorld();

        if (m_bShowHierarchy)    m_HierarchyPanel.Draw(*lSceneMgr, lWorld);
        if (m_bShowInspector)    m_InspectorPanel.Draw(lWorld);
        if (m_bShowViewport)     m_ViewportPanel.Draw(m_EditorState);
        if (m_bShowAssetBrowser) m_AssetBrowserPanel.Draw(*lSceneMgr);
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

        const OpaaxString lTempPath = OpaaxPath::ToAbsolute(PIE_TEMP_SCENE_PATH);
        std::filesystem::create_directories(
            std::filesystem::path(lTempPath.CStr()).parent_path());

        SceneSerializer::Serialize(*lSceneMgr->GetActiveScene(), lTempPath.CStr(), GetEngineApp()->GetWorld());

        SetEditorState(Editor::EEditorState::Playing);

        // Install a fresh owned runtime camera. The editor camera (non-owning observer
        // on CameraSubsystem) is structurally evicted; EditorSubsystem still owns the
        // EditorCamera through its UniquePtr so pan/zoom state survives the cycle.
        // First-frame viewport sizing flows through the existing OnResized callback.
        if (auto* lCameras = GetEngineApp()->GetSubsystem<CameraSubsystem>())
        {
            lCameras->SetActiveCamera(MakeUnique<OrthographicCamera>());
            OPAAX_CORE_INFO("EditorSubsystem: runtime camera installed for PIE.");
        }
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
            RestoreEditorCamera();
            return;
        }

        const OpaaxString lTempPath = OpaaxPath::ToAbsolute(PIE_TEMP_SCENE_PATH);
        if (!std::filesystem::exists(lTempPath.CStr()))
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — no temp snapshot found at {}",
                lTempPath.CStr());
            SetEditorState(Editor::EEditorState::Editing);
            RestoreEditorCamera();
            return;
        }

        Scene* lScene = lSceneMgr->GetActiveScene();
        World& lWorld = GetEngineApp()->GetWorld();
        // Scene stayed on the stack through PIE — its SceneID is unchanged, and
        // World::m_ActiveSceneID already matches. Wipe only this scene's entities
        // so any persistents survive the round-trip.
        lWorld.DestroyEntitiesWithSceneID(lScene->GetSceneID());
        SceneSerializer::Deserialize(*lScene, lTempPath.CStr(), lWorld);

        SetEditorState(Editor::EEditorState::Editing);
        RestoreEditorCamera();

        // PIE rebuilds entities without going through SceneManager — publish so
        // selection caches in Hierarchy / Inspector reset (entt recycles IDs).
        OnNewSceneEvent lEvent;
        m_EventBus->Publish(lEvent);
        OPAAX_CORE_INFO("EditorSubsystem - OnNewSceneEvent published from ExitPlayMode");
    }

    void EditorSubsystem::EndFrame()
    {
        ImGui::Render();
        m_UIBackend->RenderDrawData();

        const ImGuiIO& lIO = ImGui::GetIO();
        if (lIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            m_UIBackend->RenderPlatformWindows();
        }
    }

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
