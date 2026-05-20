#include "CoreEngineApp.h"

#include <cstring>
#include <filesystem>
#include <iostream>

#include "Window.h"
#include <GLFW/glfw3.h>

#include "ApplicationEvents.hpp"
#include "OpaaxPath.h"
#include "OpaaxString.hpp"
#include "OpaaxStringID.hpp"
#include "Assets/AssetRegistry.h"
#include "Assets/Loader/SceneLoader.h"
#include "Assets/Loader/TextureLoader.h"
#include "Scene/SceneAsset.h"
#include "Config/EngineConfig.h"
#include "Config/ProjectConfig.h"
#include "Editor/EditorSubsystem.h"
#include "Event/OpaaxEventDispatcher.hpp"
#include "Input/InputSubsystem.h"
#include "Log/OpaaxLog.h"
#include "Container/TPolymorphicList.hpp"
#include "Renderer/Camera2D.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderSubsystem.h"
#include "Renderer/Systems/WorldRenderSystem.h"
#include "RHI/RenderCommand.h"
#include "Scene/Scene.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"
#include "Systems/EngineSubsystem.h"
#include "World/IWorldSystem.h"
#include "World/RenderContext.h"

#include "ECS/ComponentRegistry.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/UuidComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/ParentComponent.h"

#if OPAAX_WITH_EDITOR
#include "Editor/EditorSubsystem.h"
#endif

using namespace Opaax;

namespace
{
    // Resolution order:
    //   1. CLI flag `--project <path>`
    //   2. Adjacent-to-binary: <exe-basename>/<exe-basename>.opaaxproj
    //      (matches the CMake POST_BUILD layout for each consumer project)
    //   3. Hardcoded fallback for editor / dev runs from the repo root
    OpaaxString ResolveProjectPath(int InArgc, char** InArgv)
    {
        for (int i = 1; i + 1 < InArgc; ++i)
        {
            if (std::strcmp(InArgv[i], "--project") == 0)
            {
                return OpaaxPath::ToAbsolute(InArgv[i + 1]);
            }
        }

        if (InArgc > 0 && InArgv && InArgv[0])
        {
            const std::filesystem::path lExe(InArgv[0]);
            const std::string lStem = lExe.stem().string();
            if (!lStem.empty())
            {
                OpaaxString lRel(lStem.c_str());
                lRel += "/";
                lRel += lStem.c_str();
                lRel += ".opaaxproj";
                const OpaaxString lAbs = OpaaxPath::ToAbsolute(lRel);
                std::error_code lEc;
                if (std::filesystem::exists(lAbs.CStr(), lEc))
                {
                    return lAbs;
                }
            }
        }

        return OpaaxPath::ToAbsolute("Game/Game.opaaxproj");
    }
}

CoreEngineApp::CoreEngineApp(int InArgc, char** InArgv)
{
    //The only system to be created at very first
    OpaaxLog::Init();
    OpaaxPath::Init();

    EngineConfig::Load(OpaaxPath::ToAbsolute("engine.config.json"));
    ProjectConfig::Load(ResolveProjectPath(InArgc, InArgv));

    OPAAX_CORE_TRACE("CoreEngineApp created");

    const WindowProps lProps(
        EngineConfig::WindowTitle(),
        EngineConfig::WindowWidth(),
        EngineConfig::WindowHeight());

    m_Window = UniquePtr<Window>(Opaax::Window::Create(lProps));
    m_Window->SetEventCallback([this](OpaaxEvent& Event) { DispatchEvent(Event); });

    m_DefaultRenderTarget   = MakeUnique<DefaultRenderTarget>(m_Window->GetWidth(), m_Window->GetHeight());
    m_RenderTarget          = m_DefaultRenderTarget.get();
}

CoreEngineApp::~CoreEngineApp()
{
    std::cout << "CoreEngineApp -- DESTROYED!" << std::endl;
}

void CoreEngineApp::DispatchEvent(OpaaxEvent& Event)
{
    // NOTE: Dispatch order matters.
    //   1. Engine handles WindowClose / WindowResize first — these affect loop state.
    //   2. Game layer gets a chance to consume the event via OnEvent override.
    //   3. Subsystems receive the event last (input system will live here).
    //   If a layer marks bHandled = true, subsequent layers still see the event
    //   but can choose to skip it. We do not hard-stop on bHandled — the engine
    //   always processes WindowClose regardless.
 
    OpaaxEventDispatcher lDispatcher(Event);
 
    // Engine-owned handlers — always run, not blockable by game code
    lDispatcher.Dispatch<WindowCloseEvent> ([this](WindowCloseEvent&  Event) { return OnWindowClose(Event);  });
    lDispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& Event) { return OnWindowResize(Event); });
 
    // Game layer
    if (!Event.IsHandled())
    {
        OnEvent(Event);
    }
 
    // Subsystem chain — each subsystem pre-filters by category internally.
    // InputSubsystem will register EEventCategory_Input here in Milestone 2.
    m_EngineSubsystemManager.DispatchEventAll(Event);

    // Game subsystems receive events after engine subsystems — gameplay reacts
    // to input that engine input/poll state has already latched this frame.
    m_GameSubsystemMgr.DispatchEventAll(Event);
}

bool CoreEngineApp::OnWindowClose(WindowCloseEvent& Event)
{
    OPAAX_CORE_TRACE("CoreEngineApp::OnWindowClose()");
    bIsRunning = false;
    return true;
}

void CoreEngineApp::RequestQuit() noexcept
{
    OPAAX_CORE_INFO("CoreEngineApp::RequestQuit");
    bIsRunning = false;
}

bool CoreEngineApp::OnWindowResize(WindowResizeEvent& Event)
{
    OPAAX_CORE_TRACE("CoreEngineApp::OnWindowResize() — {0}x{1}", Event.GetWidth(), Event.GetHeight());

    m_DefaultRenderTarget->OnResize(Event.GetWidth(), Event.GetHeight());

    return false;  // not consumed — game code may also want resize events
}

void CoreEngineApp::OnInitialize()
{
    OPAAX_CORE_TRACE("CoreEngineApp::OnInitialize()");
}

void CoreEngineApp::OnShutdown()
{
    OPAAX_CORE_TRACE("CoreEngineApp::OnShutdown()");

    // NOTE: Scene persistence at shutdown is a game/editor concern, not engine-mandatory.
    //   In editor builds the user drives saves explicitly (Ctrl+S / Save As).
    //   Shipping games can override OnShutdown or persist via Scene::OnUnload as needed.

    // NOTE: Assets must be destroyed before subsystems — textures and GPU resources
    //   must be freed while the GL context (owned by RenderSubsystem) is still alive.
    //OnShutdown is called before subsystem shutdown
    AssetRegistry::Shutdown();
}

void CoreEngineApp::Initialize()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Initialize()");
    
// #if OPAAX_WITH_EDITOR
//     if (IsDebugMode())
//     {
//         LaunchEditor();
//         return;
//     }
// #endif
    
    // Load engine assets
    OPAAX_CORE_TRACE("Loading engine assets...");

    AssetLoaderRegistry::Register<Texture2D>(MakeUnique<TextureLoader>());
    AssetLoaderRegistry::Register<SceneAsset>(MakeUnique<SceneLoader>());

    // Engine component types — every game-shared, scene-serializable type lives here.
    // Tag / Uuid are special-cased at the entity-json top level (display name, stable id)
    // so they're flagged bShowInAddMenu=false to keep them out of the Add menu.
    // SceneIDComponent is intentionally NOT registered — it's a runtime-only POD that
    // doesn't derive OpaaxComponentBase and never lands on disk (M2.5 design).
    ComponentRegistry::Register<ECS::TagComponent>      ("TagComponent",       /*bShowInAddMenu*/ false);
    ComponentRegistry::Register<ECS::UuidComponent>     ("UuidComponent",      /*bShowInAddMenu*/ false);
    ComponentRegistry::Register<ECS::TransformComponent>("TransformComponent");
    ComponentRegistry::Register<ECS::SpriteComponent>   ("SpriteComponent");
    ComponentRegistry::Register<ECS::ParentComponent>   ("ParentComponent",    /*bShowInAddMenu*/ false);

    // Scene types — engine knows about the base Scene under "Untitled" so the
    // editor's empty-stack fallback (SceneManager::NewScene) survives PIE Stop.
    // Game-app overrides register their own scene subclasses in OnInitialize.
    SceneFactory::Register("Untitled", []() -> UniquePtr<Scene> { return MakeUnique<Scene>("Untitled"); });

    const OpaaxString lEngineManifest =
        OpaaxPath::ToAbsolute(EngineConfig::EngineManifestRelPath());
    const OpaaxString lProjectManifest =
        OpaaxPath::ToAbsolute(ProjectConfig::AssetsManifestRelPath());

    AssetManifest::LoadFile(lEngineManifest);
    AssetManifest::LoadFile(lProjectManifest);
    
    m_EngineSubsystemManager.RegisterSubsystem<RenderSubsystem>(this);
    m_EngineSubsystemManager.RegisterSubsystem<Camera2D>(this);

    m_EngineSubsystemManager.RegisterSubsystem<InputSubsystem>(this);

    m_EngineSubsystemManager.RegisterSubsystem<SceneManager>(this);

#if OPAAX_WITH_EDITOR
    m_EngineSubsystemManager.RegisterSubsystem<EditorSubsystem>(this);
#endif

    // Default world-render system. Games append more in OnInitialize (called right after);
    // dispatch order = registration order.
    TPolymorphicList<IWorldSystem>::Register(MakeUnique<WorldRenderSystem>());

    // Call derived class initialization
    OnInitialize();

    bIsRunning = true;
}

void CoreEngineApp::Startup()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Startup()");

    m_EngineSubsystemManager.StartupAll();
    m_GameSubsystemMgr.StartupAll();

    OnStartup();

#if OPAAX_WITH_EDITOR
    // If game code didn't push a scene, hand the editor a blank "Untitled"
    // so a brand-new project is immediately editable (Unreal-style default).
    if (SceneManager* lSceneMgr = GetSceneManager(); lSceneMgr && lSceneMgr->IsEmpty())
    {
        OPAAX_CORE_INFO("CoreEngineApp::Startup — no scene pushed; opening Untitled.");
        lSceneMgr->NewScene();
    }
#endif
}

void CoreEngineApp::Run()
{
    Initialize();
    Startup();
 
    if (!m_Window)
    {
        OPAAX_CORE_ERROR("CoreEngineApp::Run() — window is null, aborting...");
        Shutdown();
        return;
    }
 
    OPAAX_CORE_TRACE("CoreEngineApp::Run() — entering game loop");

    // NOTE: Fixed timestep accumulator pattern.
    //   Physics/logic ticks at a stable 60 Hz regardless of render frame rate.
    //   Render receives the interpolation alpha so it can lerp between physics steps.
    constexpr double FIXED_DELTA_TIME    = 1.0 / 60.0;
    constexpr double MAX_FRAME_DELTA     = 0.25;  // spiral-of-death guard
 
    double lAccumulator = 0.0;
    double lLastTime    = glfwGetTime();
    
    while (bIsRunning)
    {
        // ----------------------------------------------------------------
        // 1. windows events (input, close, etc..)
        // ----------------------------------------------------------------
        m_Window->PollEvents();

        // ----------------------------------------------------------------
        // 1.1 close event called?
        // ----------------------------------------------------------------
        bIsRunning = !m_Window->ShouldClose();
        if (!bIsRunning)
        {
            break;
        }

        // ----------------------------------------------------------------
        // 2. Time
        // ----------------------------------------------------------------
        const double lTimeNow   = glfwGetTime();
        double       lDeltaTime = lTimeNow - lLastTime;
        lLastTime               = lTimeNow;
 
        if (lDeltaTime > MAX_FRAME_DELTA) { lDeltaTime = MAX_FRAME_DELTA; }
 
        lAccumulator += lDeltaTime;
        
        // ----------------------------------------------------------------
        // 2.0 PIE gating — play-only subsystems tick only when Playing.
        //   Editor builds: derived from EditorSubsystem state.
        //   Non-editor builds: always true (gameplay always runs).
        // ----------------------------------------------------------------
#if OPAAX_WITH_EDITOR
        const bool bAllowPlayOnly =
            (m_EngineSubsystemManager.GetSubsystem<EditorSubsystem>()->GetEditorState()
                == Editor::EEditorState::Playing);
#else
        constexpr bool bAllowPlayOnly = true;
#endif

        // ----------------------------------------------------------------
        // 2.1 Variable update — gameplay, animations, AI
        //   Engine layer first, then game layer. Game subsystems whose
        //   IsPlayOnly() returns true are skipped when bAllowPlayOnly is false.
        // ----------------------------------------------------------------
        m_EngineSubsystemManager.UpdateAll(lDeltaTime, bAllowPlayOnly);
        m_GameSubsystemMgr.UpdateAll(lDeltaTime, bAllowPlayOnly);
        OnUpdate(lDeltaTime);

        // ----------------------------------------------------------------
        // 2.2 Fixed update — physics, at stable 60 Hz
        // ----------------------------------------------------------------
        while (lAccumulator >= FIXED_DELTA_TIME)
        {
            m_EngineSubsystemManager.FixedUpdateAll(FIXED_DELTA_TIME, bAllowPlayOnly);
            m_GameSubsystemMgr.FixedUpdateAll(FIXED_DELTA_TIME, bAllowPlayOnly);
            OnFixedUpdate(FIXED_DELTA_TIME);
            lAccumulator -= FIXED_DELTA_TIME;
        }

        // ----------------------------------------------------------------
        // 2.3 Render — interpolated between last and next physics step.
        //   Order matters: engine OnRender writes the world to the bound render target
        //   (backbuffer in release, ViewportPanel FBO in editor). RenderAll runs after
        //   so editor/overlay subsystems sample the fresh target this frame, not last.
        // ----------------------------------------------------------------
        const double lAlpha = lAccumulator / FIXED_DELTA_TIME;

        OnRender(lAlpha);
        m_EngineSubsystemManager.RenderAll(lAlpha);
        
        // ----------------------------------------------------------------
        // 3. Swap AFTER render, always last
        // ----------------------------------------------------------------
        m_Window->SwapBuffers();
    }
    
    OPAAX_CORE_TRACE("CoreEngineApp::Run() — game loop exited");

    Shutdown();
}

void CoreEngineApp::Shutdown()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Shutdown()");
    
    bIsRunning = false;
    
    OnShutdown();

    // Game subsystems shut down first — they may still call into engine services
    // (Renderer, World, AssetRegistry) during their Shutdown.
    m_GameSubsystemMgr.ShutdownAll();

    // Component registry holds editor drawer pointers — clear before the editor
    // subsystem dies so drawers don't outlive their owning subsystem. EditorSubsystem::Shutdown
    // also calls Clear() in editor builds (idempotent — second call is a no-op);
    // this call covers release builds where EditorSubsystem doesn't exist.
    ComponentRegistry::Clear();

    // Drop scene factories before subsystems die — captured lambdas could hold
    // references into game-side types whose libraries unload after this point.
    SceneFactory::Clear();

    m_EngineSubsystemManager.ShutdownAll();

    OpaaxLog::Shutdown();
}

void CoreEngineApp::OnRender(double AlphaPhysicStep)
{
    IRenderTarget& lTarget = GetRenderTarget();
    lTarget.Bind();

    RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    RenderCommand::Clear();

    auto* lCamera = GetSubsystem<Camera2D>();
    Renderer2D::Begin(*lCamera);

    if (GetSceneManager()->GetActiveScene())
    {
        World& lWorld = GetWorld();
        const RenderContext lCtx{ *lCamera, AlphaPhysicStep };
        for (const auto& lSystem : TPolymorphicList<IWorldSystem>::GetAll())
        {
            lSystem->OnRender(lWorld, lCtx);
        }
    }

    Renderer2D::End();
    lTarget.Unbind();
}

World& CoreEngineApp::GetWorld() noexcept
{
    return m_World;
}

Opaax::SceneManager* CoreEngineApp::GetSceneManager() noexcept
{
    return m_EngineSubsystemManager.GetSubsystem<SceneManager>();
}

#if OPAAX_WITH_EDITOR
void CoreEngineApp::LaunchEditor()
{
    OPAAX_CORE_TRACE("[EDITOR MODE] Launching editor...");
    
    // TODO: Initialize editor UI
}

bool CoreEngineApp::IsDebugMode() const {
#ifdef _DEBUG
    return true;
#else
    return false;
#endif
}
#endif