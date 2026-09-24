#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/AppServiceLocator.h"
#include "World/WorldSpec.h"   // GetStartupWorldSpec returns one by value
#include "Core/Events/Event.h"

namespace Opaax
{
    // NOTE: ModuleRegistrar is an ENGINE-layer type (it fronts the engine registries) and is
    // held by pointer on purpose — including its header here would pull World/ and entt into
    // every consumer of this Application header. Composition roots that actually register
    // something include Engine/Modules/ModuleRegistrar.h themselves.
    class ModuleRegistrar;

    class IEngine;
    class IConfigSystem;
    class IProjectManager;
    class IPlatform;
    class IPaths;
    class ILogger;
    class IJobSystem;
    class IStatsService;
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
        virtual TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv);
    private:
        IConfigSystem&      BootConfigSystem(const IPaths& Paths);
        IProjectManager&    BootProjectManager(const IPaths& Paths);
        IJobSystem&         BootJobSystem();

        /**
         * Provide the stats service — or deliberately DO NOT, which is how stats are turned off:
         * the locator's null object (I3) is the disabled state, so there is no second flag.
         *
         * Config-driven, hence after BootConfigSystem (L1's locked order, the same reason the job
         * system's worker count is).
         */
        IStatsService&      BootStatsService(IConfigSystem& ConfigSystem);
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
         * Called once per RunApplication iteration.
         * Give a change to child app to override the order of the frame (i.e) The editor need to know about layout/ui to render into the Viewport panel
         */
        virtual void TickFrame();

    public:
        /**
         * The app loop
         */
        void RunApplication();
        
        /**
         * Mainly Window event to dispatch to other services. Virtual so a composition root (the editor)
         * can intercept events at the window-callback site BEFORE the base enqueues them to the engine bus.
         * Base is unchanged; runtime has no override.
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
         * 
         */
        void PopulateEngineRegistries();
        
        /**
         * Fires in EngineStartup AFTER RegisterModules (game module registered) and BEFORE the
         * startup world exists.
         */
        virtual void OnModulesRegistered() {}
        
        // End Modules
        // =============================================================================
        
        // =============================================================================
        // Startup world
        /**
         * @return The Startup world spec base on project configs
         */
        virtual WorldSpec GetStartupWorldSpec() const;

        // End Startup world
        // =============================================================================
        
        // =============================================================================
        // Events Handles
    protected:
        virtual void HandleApplicationEvent(EventDispatcher& Dispatcher, Event& InEvent);
        virtual void HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent);
        virtual void UnknownEvent(EventDispatcher& Dispatcher, Event& InEvent);
        // End Events Handles
        // =============================================================================
        
        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        static AppServiceLocator& GetServices() noexcept { return m_Services; }
        
        template<typename T>
        static T& GetAppService(){ return GetServices().Get<T>(); }

        // Convenience accessors — never null (the locator returns the null object).
        IPlatform&          Platform();
        IPaths&             Paths();
        ILogger&            Logger();
        IProjectManager&    ProjectManager();
        IConfigSystem&      ConfigSystem();
        IJobSystem&         JobSystem();
        IStatsService&      Stats();
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
        
        TUniquePtr<ModuleRegistrar> m_ModuleRegistrar;

        static AppServiceLocator m_Services;
    };
}
