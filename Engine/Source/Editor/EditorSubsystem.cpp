#include "EditorSubsystem.h"
#include "GLFW/glfw3.h"

#if OPAAX_WITH_EDITOR

#include <filesystem>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "Core/CoreEngineApp.h"
#include "Core/OpaaxPath.h"
#include "Core/Window.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"
#include "Core/Log/OpaaxLog.h"
#include "Renderer/Camera2D.h"
#include "RHI/RenderCommand.h"

#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/Types/SceneTypeActions.h"
#include "Editor/Assets/Types/Texture2DTypeActions.h"
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
    // PIE — temp snapshot files used for Play→Stop scene-stack restore
    //
    // PIE_TEMP_MANIFEST_PATH stores the stack metadata (one entry per scene with
    // its SceneFactory name + the path to its per-scene entity dump). Per-scene
    // entity dumps live alongside as PlaySession_<i>.json. On Stop we pop the
    // entire current stack and rebuild from the manifest via SceneFactory —
    // tolerates games that Push/Replace scenes during PIE.
    // =============================================================================
    static constexpr const char* PIE_TEMP_MANIFEST_PATH    = "EditorTemp/PlaySession.json";
    static constexpr const char* PIE_TEMP_SCENE_DIR        = "EditorTemp";
    static constexpr const char* PIE_TEMP_PERSISTENTS_PATH = "EditorTemp/PlaySession_persistent.json";
    static constexpr int         PIE_MANIFEST_VERSION      = 1;

    static OpaaxString MakePerScenePath(Uint32 InIndex)
    {
        char lBuf[64];
        std::snprintf(lBuf, sizeof(lBuf), "%s/PlaySession_%u.json", PIE_TEMP_SCENE_DIR, InIndex);
        return OpaaxPath::ToAbsolute(lBuf);
    }

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

        // Editor event bus: panels register handlers here. Tokens stored as panel
        // members; RAII unsubscribe at panel destruction. Order is not meaningful —
        // Publish dispatches in registration order per event type.
        m_MainMenuBar.OnSubscribe(*m_EventBus);
        m_HierarchyPanel.OnSubscribe(*m_EventBus);
        m_InspectorPanel.OnSubscribe(*m_EventBus);
        m_AssetBrowserPanel.OnSubscribe(*m_EventBus);

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
        Editor::AssetTypeRegistry::Register(MakeUnique<Editor::SceneTypeActions>(GetEngineApp()));

        OPAAX_CORE_TRACE("EditorSubsystem: asset type actions registered.");
    }

    void EditorSubsystem::RegisterComponentDrawers()
    {
        // Component types themselves are registered by CoreEngineApp::Initialize.
        // Here we only attach custom drawers — components without one fall back to
        // ComponentRegistry's default read-only json renderer.
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
        ComponentRegistry::Clear();

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
        // Engine's OnRender clears the ViewportPanel FBO; ImGui draws to the default framebuffer,
        // which would otherwise accumulate stale pixels behind any moving panel (PassthruCentralNode
        // + NoBackground dockspace leak last frame's content through every gap).
        RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
        RenderCommand::Clear();

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

        // MainMenuBar can mutate the scene stack (Stop → ExitPlayMode pops+rebuilds,
        // Open/New/LoadScene swap the active scene). Capture the post-Draw state so
        // downstream panels never dereference a Scene* the toolbar just destroyed.
        m_MainMenuBar.Draw(*this);

        Scene* lActiveScene = lSceneMgr ? lSceneMgr->GetActiveScene() : nullptr;
        World* lWorld       = lActiveScene ? &GetEngineApp()->GetWorld() : nullptr;

        if (m_bShowHierarchy)    m_HierarchyPanel.Draw(lActiveScene, lWorld);
        if (m_bShowInspector)    m_InspectorPanel.Draw(lWorld);
        if (m_bShowViewport)     m_ViewportPanel.Draw(m_EditorState);
        if (m_bShowAssetBrowser) m_AssetBrowserPanel.Draw(lSceneMgr);
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
        if (!lSceneMgr || lSceneMgr->GetStackDepth() == 0)
        {
            OPAAX_CORE_WARN("EditorSubsystem::EnterPlayMode — empty scene stack, aborting.");
            return;
        }

        const OpaaxString lManifestPath = OpaaxPath::ToAbsolute(PIE_TEMP_MANIFEST_PATH);
        std::filesystem::create_directories(
            std::filesystem::path(lManifestPath.CStr()).parent_path());

        // Snapshot every scene on the stack, bottom-to-top, into its own file.
        // The manifest records (scene-name → per-scene-file) so ExitPlayMode can
        // rebuild the stack via SceneFactory even if PIE pushed/popped scenes.
        nlohmann::json lManifest;
        lManifest["version"] = PIE_MANIFEST_VERSION;
        lManifest["stack"]   = nlohmann::json::array();

        const Uint32 lDepth = lSceneMgr->GetStackDepth();
        for (Uint32 i = 0; i < lDepth; ++i)
        {
            Scene* lScene = lSceneMgr->GetSceneAt(i);
            OPAAX_CORE_ASSERT(lScene)

            const OpaaxString lScenePath = MakePerScenePath(i);
            SceneSerializer::Serialize(*lScene, lScenePath.CStr(), GetEngineApp()->GetWorld());

            nlohmann::json lEntry;
            lEntry["name"] = lScene->GetName().CStr();
            lEntry["file"] = lScenePath.CStr();
            lManifest["stack"].push_back(lEntry);
        }

        std::ofstream lFile(lManifestPath.CStr());
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("EditorSubsystem::EnterPlayMode — cannot write PIE manifest '{}'.",
                lManifestPath.CStr());
            return;
        }
        lFile << lManifest.dump(4);

        // Persistent-bucket snapshot — entities created via World::CreatePersistentEntity
        // (SceneID == 0) are not inside any scene's bucket and must be captured separately
        // so PIE Stop can roll back any mutations made during Play.
        const OpaaxString lPersistentPath = OpaaxPath::ToAbsolute(PIE_TEMP_PERSISTENTS_PATH);
        SceneSerializer::SerializePersistents(GetEngineApp()->GetWorld(), lPersistentPath.CStr());

        OPAAX_CORE_INFO("EditorSubsystem::EnterPlayMode — snapshotted {} scene(s).", lDepth);
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
        OPAAX_CORE_INFO("EditorSubsystem::ExitPlayMode — entered (state={})",
            Editor::EditorStateToString(m_EditorState));

        if (m_EditorState == Editor::EEditorState::Editing)
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — already in Editing.");
            return;
        }

        SceneManager* lSceneMgr = GetEngineApp() ? GetEngineApp()->GetSceneManager() : nullptr;
        if (!lSceneMgr)
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — no SceneManager.");
            SetEditorState(Editor::EEditorState::Editing);
            return;
        }

        const OpaaxString lManifestPath = OpaaxPath::ToAbsolute(PIE_TEMP_MANIFEST_PATH);
        if (!std::filesystem::exists(lManifestPath.CStr()))
        {
            OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — no PIE manifest found at {}",
                lManifestPath.CStr());
            SetEditorState(Editor::EEditorState::Editing);
            return;
        }

        // Read the manifest before we touch the stack — if anything fails we
        // leave the current stack alone rather than tearing it down half-way.
        nlohmann::json lManifest;
        {
            std::ifstream lIn(lManifestPath.CStr());
            try
            {
                lIn >> lManifest;
            }
            catch (const nlohmann::json::parse_error& e)
            {
                OPAAX_CORE_ERROR("EditorSubsystem::ExitPlayMode — manifest parse failed: {}", e.what());
                SetEditorState(Editor::EEditorState::Editing);
                return;
            }
        }

        if (!lManifest.contains("stack") || !lManifest["stack"].is_array())
        {
            OPAAX_CORE_ERROR("EditorSubsystem::ExitPlayMode — malformed manifest (missing 'stack' array).");
            SetEditorState(Editor::EEditorState::Editing);
            return;
        }

        // Tear down the current stack — destroys every play-mode scene entity.
        // Persistent-tagged entities (SceneID == 0) pass through Pop() untouched
        // (DestroyEntitiesWithSceneID never matches PersistentSceneID); they are
        // rolled back further down via the persistent snapshot dance.
        OPAAX_CORE_INFO("EditorSubsystem::ExitPlayMode — popping {} scene(s) from current stack.",
            lSceneMgr->GetStackDepth());
        while (lSceneMgr->GetStackDepth() > 0)
        {
            lSceneMgr->Pop();
        }

        // Rebuild bottom-to-top from the manifest. SceneFactory provides the
        // C++ class identity for each entry; Push runs OnLoad (which may spawn
        // initial entities); we wipe those and Deserialize the snapshotted set.
        World& lWorld = GetEngineApp()->GetWorld();
        Uint32 lRestored = 0;

        for (const auto& lEntry : lManifest["stack"])
        {
            if (!lEntry.contains("name") || !lEntry.contains("file"))
            {
                OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — skipping malformed manifest entry.");
                continue;
            }

            const std::string lNameStr = lEntry["name"].get<std::string>();
            const std::string lFileStr = lEntry["file"].get<std::string>();
            const OpaaxStringID lNameID(lNameStr.c_str());

            OPAAX_CORE_INFO("EditorSubsystem::ExitPlayMode — rebuilding scene '{}' from '{}'.",
                lNameStr, lFileStr);

            UniquePtr<Scene> lNew = SceneFactory::Create(lNameID);
            if (!lNew)
            {
                OPAAX_CORE_ERROR("EditorSubsystem::ExitPlayMode — no SceneFactory entry for '{}'; skipping.",
                    lNameStr);
                continue;
            }

            Scene* lRawPtr = lNew.get();
            lSceneMgr->Push(std::move(lNew));

            // OnLoad may have spawned starter entities (e.g. ShmupGameScene's
            // player). Wipe them — the snapshot is the authoritative state.
            lWorld.DestroyEntitiesWithSceneID(lRawPtr->GetSceneID());

            if (!SceneSerializer::Deserialize(*lRawPtr, lFileStr.c_str(), lWorld))
            {
                OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — Deserialize failed for '{}'.", lNameStr);
            }
            ++lRestored;
        }

        // Roll back the persistent bucket. The Pop loop above intentionally
        // does not touch PersistentSceneID entities (that's the persistence
        // guarantee for runtime); for PIE we own the rollback semantic and
        // restore them from the EnterPlayMode snapshot. Scenes are restored
        // first so any persistent parent_uuid links can resolve against
        // already-deserialized scene entities.
        const OpaaxString lPersistentPath = OpaaxPath::ToAbsolute(PIE_TEMP_PERSISTENTS_PATH);
        if (std::filesystem::exists(lPersistentPath.CStr()))
        {
            lWorld.DestroyEntitiesWithSceneID(World::PersistentSceneID);
            if (!SceneSerializer::DeserializePersistents(lWorld, lPersistentPath.CStr()))
            {
                OPAAX_CORE_WARN("EditorSubsystem::ExitPlayMode — persistent restore failed.");
            }
        }
        else
        {
            OPAAX_CORE_INFO("EditorSubsystem::ExitPlayMode — no persistent snapshot, skipping rollback.");
        }

        OPAAX_CORE_INFO("EditorSubsystem::ExitPlayMode — restored {} scene(s) from snapshot.", lRestored);
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
