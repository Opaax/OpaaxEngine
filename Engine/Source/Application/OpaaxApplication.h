#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/AppServiceLocator.h"
#include "Application/Modules/ModuleRegistrar.h"

namespace Opaax
{
    class IEngine;
    class IConfigSystem;
    class IProjectManager;
    class IPlatform;
    class IPaths;
    class ILogger;
    class IJobSystem;
    class IWindowManager;
    class Event;

    // =============================================================================
    // OpaaxApplication — base application host. Owns the AppServiceLocator and boots
    // the app-level services (Platform, Paths, ...) in dependency order. Successor to
    // CoreEngineApp's host role; the engine itself will become a service (IEngine).
    // =============================================================================
    class OPAAX_API OpaaxApplication
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        OpaaxApplication(int InArgc, char** InArgv);
        virtual ~OpaaxApplication();

        // =============================================================================
        // Copy - Move = delete
        // =============================================================================
    private:
        OpaaxApplication(const OpaaxApplication&) = delete;
        OpaaxApplication& operator=(const OpaaxApplication&) = delete;

        OpaaxApplication(OpaaxApplication&&) = delete;
        OpaaxApplication& operator=(OpaaxApplication&&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
        
        // =============================================================================
        // Bootstrap
    private:
        IPlatform&          BootPlatform();
        IPaths&             BootPaths();
        ILogger&            BootLogger(IPaths& Paths);

    protected:
        /**
         * Factory seam for the Paths service. Editor vs Standalone do not need the same paths
         */
        virtual UniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv);
    private:
        IConfigSystem&      BootConfigSystem(const IPaths& Paths);
        IProjectManager&    BootProjectManager(const IPaths& Paths);
        IJobSystem&         BootJobSystem();
        IWindowManager&     BootWindowManager();
        IEngine&            BootEngine();
        
    protected:
        /**
         * IConfigSystem::Get also register is not registered yet
         * But here you can Pre register config at application boot
         * @param ConfigSystem
         */
        virtual void PreRegisterConfig(IConfigSystem& ConfigSystem);

        /**
         * Child app can add their services here
         */
        virtual void OnProvideServices(AppServiceLocator& InServices) {}

    public:
        /**
         * Provide the app-level services into the locator, in dependency order.
         */
        void Bootstrap();
        
        // End Bootstrap
        // =============================================================================
        
        // =============================================================================
        // Initialization
    private:
        void CreateApplicationWindow();
        
    protected:
        virtual void OnInitializeApplication();
        
    public:
        /**
         * Initialize the app
         */
        void InitializeApplication();
        
        // End Initialization
        // =============================================================================
        
        // =============================================================================
        // Flow
    protected:
        /**
         * Seam (Editor.md D1): the per-frame body, called once per RunApplication iteration.
         * Base = the engine frame (Engine().Loop()), so runtime is byte-for-byte unchanged. The
         * editor overrides it as: UI begin -> Engine().Loop() -> UI end (S10). Present stays out
         * of here — the host presents after TickFrame() (S7).
         */
        virtual void TickFrame();

    public:
        /**
         * The app loop
         */
        void RunApplication();
        
        /**
         * Mainly Window event to dispatch to other services. Virtual so a composition root (the editor)
         * can intercept events at the window-callback site BEFORE the base enqueues them to the engine
         * bus (Editor.md "Event ordering", S11). Base is unchanged; runtime has no override.
         * @param InEvent
         */
        virtual void OnEvent(Event& InEvent);
        
        /**
         * Make the application shutdown explicit
         */
        void ShutdownApplication();
        
        // End Flow
        // =============================================================================
        
        // =============================================================================
        // Engine
        /**
        * Before engine start
        * Engine subsystem not start yet
        */
        virtual void PreEngineStartup() {}
        
        /***/
        void EngineStartup();

        /**
         * After engine start
         * Engine subsystem has start
         */
        virtual void PostEngineStartup(){}
        
        /***/
        void EngineTeardown();
        
        // Engine
        // =============================================================================
        
        // =============================================================================
        // Modules
    protected:
        /**
         * 
         */
        virtual void RegisterModules(ModuleRegistrar& InRegistrar) {}
        
        /**
         * Fires in EngineStartup AFTER RegisterModules (game module registered) and BEFORE Engine().Startup()
         */
        virtual void OnModulesRegistered() {}
        
        // End Modules
        // =============================================================================
        
        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        static AppServiceLocator& Services() noexcept { return m_Services; }
        
        template<typename T>
        static T& GetAppService(){ return Services().Get<T>(); }

        // Convenience accessors — never null (the locator returns the null object).
        IPlatform&          Platform();
        IPaths&             Paths();
        ILogger&            Logger();
        IProjectManager&    ProjectManager();
        IConfigSystem&      ConfigSystem();
        IJobSystem&         JobSystem();
        IWindowManager&     WindowManager();
        IEngine&            Engine();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        int    m_Argc = 0;
        char** m_Argv = nullptr;
        
        bool bHasBootstrap      = false;
        bool bHasInitialized    = false;
        bool bIsRunning         = false;
        bool bHasShutdown       = false;

        // Populated at OnRegisterModules (D9). At M0 it only records registration counts; from M3/M4
        // its routes forward to the real engine registries. Owned by the app so the record survives
        // boot for inspection/tests.
        ModuleRegistrar m_ModuleRegistrar;

        static AppServiceLocator m_Services;
    };
}
