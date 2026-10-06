#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/AppServiceLocator.h"
#include "World/WorldSpec.h"   // GetStartupWorldSpec returns one by value
#include "Core/Events/Event.h"

namespace Opaax
{
    // Forward-declared to keep World/ and entt out of this header.
    class ModuleRegistrar;

    class IEngine;
    class IConfigSystem;
    class IProjectManager;
    class IPlatform;
    class IPaths;
    class IJobSystem;
    class IWindowManager;
    class Event;

    // =============================================================================
    // OpaaxApplication — base application host. Owns the AppServiceLocator and boots
    // the app-level services (Platform, Paths, ...) in dependency order.
    // =============================================================================
    class OpaaxApplication
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

    protected:
        /**
         * Creates the Paths service. Editor and standalone use different paths.
         */
        virtual TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv);
    private:
        IConfigSystem&      BootConfigSystem(const IPaths& Paths);
        IProjectManager&    BootProjectManager(const IPaths& Paths);
        IJobSystem&         BootJobSystem();

        /** Enables the Profiler: always in dev builds, from config in ship builds. */
        void                BootProfiler(IConfigSystem& ConfigSystem);
        IWindowManager&     BootWindowManager();
        IEngine&            BootEngine();
        
    protected:
        /**
         * Pre-registers configs at boot. IConfigSystem::Get also registers on first use.
         */
        virtual void PreRegisterConfig(IConfigSystem& ConfigSystem);

        /**
         * Child apps add their own services here.
         */
        virtual void OnProvideServices(AppServiceLocator& InServices) {}

    public:
        /**
         * Provides the app-level services, in dependency order.
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
         * Initializes the app.
         */
        void InitializeApplication();
        
        // End Initialization
        // =============================================================================
        
        // =============================================================================
        // Flow
    protected:
        /**
         * Called once per loop iteration. Child apps can reorder the frame
         * (the editor renders into its viewport panel).
         */
        virtual void TickFrame();

    public:
        /**
         * The app loop.
         */
        void RunApplication();
        
        /**
         * Receives window events and forwards them to the engine.
         * Child apps (the editor) can intercept them first.
         */
        virtual void OnEvent(Event& InEvent);
        
        /**
         * Shuts the application down.
         */
        void ShutdownApplication();
        
        // End Flow
        // =============================================================================
        
        // =============================================================================
        // Engine
        /**
         * Called before the engine subsystems start.
         */
        virtual void PreEngineStartup() {}
        
        void EngineStartup();

        /**
         * Called after the engine subsystems started.
         */
        virtual void PostEngineStartup(){}
        
        void EngineTeardown();
        
        // Engine
        // =============================================================================
        
        // =============================================================================
        // Modules
    protected:
        /**
         * Registers the game modules into the engine registries.
         */
        virtual void RegisterModules(ModuleRegistrar& InRegistrar) {}
        
        void PopulateEngineRegistries();
        
        /**
         * Called after RegisterModules, before the startup world is created.
         */
        virtual void OnModulesRegistered() {}
        
        // End Modules
        // =============================================================================
        
        // =============================================================================
        // Startup world
        /**
         * @return The startup world spec, from the project config
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

        // Never null: the locator returns a null object when a service is missing.
        IPlatform&          Platform();
        IPaths&             Paths();
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
        
        TUniquePtr<ModuleRegistrar> m_ModuleRegistrar;

        static AppServiceLocator m_Services;
    };
}
