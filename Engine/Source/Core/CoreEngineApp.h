#pragma once

#include "EngineAPI.h"
#include "OpaaxTypes.h"
#include "Renderer/RenderTarget.hpp"
#include "Systems/EngineSubsystem.h"
#include "Systems/GameSubsystem.h"
#include "World/World.h"

namespace Opaax
{
    using namespace ECS;

    class WindowResizeEvent;
    class WindowCloseEvent;
    class OpaaxEvent;
    class Window;
    class SceneManager;
    class Window;

    class OPAAX_API CoreEngineApp
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        CoreEngineApp(int InArgc, char** InArgv);
        virtual ~CoreEngineApp();
    
    private:
        // Delete copy constructor and copy assignment operator
        CoreEngineApp(const CoreEngineApp&) = delete;
        CoreEngineApp& operator=(const CoreEngineApp&) = delete;
    
        // Delete move constructor and move assignment operator
        CoreEngineApp(CoreEngineApp&&) = delete;
        CoreEngineApp& operator=(CoreEngineApp&&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void DispatchEvent(OpaaxEvent& Event);
        bool OnWindowClose(WindowCloseEvent& Event);
        bool OnWindowResize(WindowResizeEvent& Event);

    protected:
        virtual void OnInitialize();
        virtual void OnStartup() {}
        virtual void OnUpdate(double DeltaTime) {}
        virtual void OnFixedUpdate(double FixedDeltaTime) {}
        virtual void OnRender(double AlphaPhysicStep);
        virtual void OnShutdown();
        virtual bool OnEvent(OpaaxEvent& Event) { return false; }
        
    public:
        void Initialize();
        void Startup();
        void Run();
        void Shutdown();
        void RequestQuit() noexcept;

        //------------------------------------------------------------------------------
        //  Get - Set
        
        Window&         GetWindow() const { return *m_Window; }
        World&          GetWorld() noexcept;
        SceneManager*   GetSceneManager() noexcept;
        
        /**
         * 
         * @return 
         */
        FORCEINLINE IRenderTarget& GetRenderTarget() noexcept
        {
            OPAAX_CORE_ASSERT(m_RenderTarget != nullptr)
            return *m_RenderTarget;
        }
        
        /**
         * 
         * @param InTarget 
         */
        void SetRenderTarget(IRenderTarget* InTarget) noexcept
        {
            OPAAX_CORE_ASSERT(InTarget != nullptr)
            m_RenderTarget = InTarget;
        }

        template<typename T>
        T* GetSubsystem()
        {
            T* lResult = m_EngineSubsystemManager.GetSubsystem<T>();
            OPAAX_CORE_ASSERT(lResult)
            return lResult;
        }

        template<typename T>
        const T* GetSubsystem() const
        {
            const T* lResult = m_EngineSubsystemManager.GetSubsystem<T>();
            OPAAX_CORE_ASSERT(lResult)
            return lResult;
        }

        // =============================================================================
        // Game subsystems
        //
        // Parallel layer to engine subsystems — owns gameplay code (player control,
        // AI, collision rules). Manager is PIE-aware via IsPlayOnly() so gameplay
        // ticks only during Play, not edit mode. Register from the game-app's
        // OnInitialize override via RegisterGameSubsystem<T>(this).
        // =============================================================================

        template<typename T, typename... Args>
        void RegisterGameSubsystem(Args&&... InArgs)
        {
            m_GameSubsystemMgr.RegisterSubsystem<T>(std::forward<Args>(InArgs)...);
        }

        template<typename T>
        T* GetGameSubsystem()
        {
            T* lResult = m_GameSubsystemMgr.GetSubsystem<T>();
            OPAAX_CORE_ASSERT(lResult)
            return lResult;
        }

        template<typename T>
        const T* GetGameSubsystem() const
        {
            const T* lResult = m_GameSubsystemMgr.GetSubsystem<T>();
            OPAAX_CORE_ASSERT(lResult)
            return lResult;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool bIsRunning = false;

        // Previous frame's play-gate state — drives the OnPlayBegin/OnPlayEnd edges.
        bool bWasPlaying = false;
        UniquePtr<Window> m_Window;
        EngineSubsystemMgr m_EngineSubsystemManager;
        GameSubsystemMgr   m_GameSubsystemMgr;

        IRenderTarget*              m_RenderTarget        = nullptr;
        UniquePtr<DefaultRenderTarget> m_DefaultRenderTarget;

        // Persistent World — owns all entities + (post-M2.5) the scene stack.
        // GetWorld() returns this directly. Scene::m_World is orphaned and
        // gets removed in Step 5.
        World                       m_World;

#if OPAAX_WITH_EDITOR
        void LaunchEditor();
        bool IsDebugMode() const;
#endif
    };
}
