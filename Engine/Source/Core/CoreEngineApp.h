#pragma once

#include "EngineAPI.h"
#include "OpaaxTypes.h"
#include "Systems/EngineSubsystem.h"
#include "World/World.h"

namespace Opaax
{
    using namespace ECS;
    
    class WindowResizeEvent;
    class WindowCloseEvent;
    class OpaaxEvent;
    class Window;

    class OPAAX_API CoreEngineApp
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        CoreEngineApp();
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
        virtual void OnRender(double AlphaPhysicStep) {}
        virtual void OnShutdown();
        virtual bool OnEvent(OpaaxEvent& Event) { return false; }
        
    public:
        void Initialize();
        void Startup();
        void Run();
        void Shutdown();

        //------------------------------------------------------------------------------
        //  Get - Set
        
        Window& GetWindow() const { return *m_Window; }
        World& GetWorld() noexcept { return m_World; }

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
        // Members
        // =============================================================================
    private:
        bool bIsRunning = false;
        UniquePtr<Window> m_Window;
        EngineSubsystemMgr m_EngineSubsystemManager;

        World m_World;

#if OPAAX_WITH_EDITOR
        void LaunchEditor();
        bool IsDebugMode() const;
#endif
    };
}
