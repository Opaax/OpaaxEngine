#pragma once

#include "EngineAPI.h"
#include "OpaaxTypes.h"
#include "Systems/EngineSubsystem.h"

namespace Opaax
{
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
    public:
        void Initialize();
        void Startup();
        void Run();
        void Shutdown();

        /*----------------------------- Get - Set -------------------------------*/
        
        Window& GetWindow() const { return *m_Window; }

    protected:
        virtual void OnInitialize() {}
        virtual void OnStartup() {}
        virtual void OnUpdate(double DeltaTime) {}
        virtual void OnFixedUpdate(double FixedDeltaTime) {}
        virtual void OnRender(double AlphaPhysicStep) {}
        virtual void OnShutdown() {}

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool bIsRunning = false;
        UniquePtr<Window> m_Window;
        EngineSubsystemMgr m_EngineSubsystemManager;

#if OPAAX_WITH_EDITOR
        void LaunchEditor();
        bool IsDebugMode() const;
#endif
    };
}
