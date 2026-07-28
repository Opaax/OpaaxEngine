#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/AppServiceLocator.h"
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
         * Fires in EngineStartup AFTER RegisterModules (game module registered) and BEFORE the
         * startup world exists.
         */
        virtual void OnModulesRegistered() {}

        /**
         * Create the world the application starts in — the LAST step of engine boot, once every
         * subsystem is up and every module has registered.
         *
         * Deliberately not done by `WorldManager::Startup`: starting a subsystem brings up
         * infrastructure, whereas choosing a world is content, and the choice belongs to the host.
         * It also has to happen here for a mechanical reason — the first `CreateWorld` SEALS
         * `ComponentRegistry`, so a world created any earlier would lock out the game module.
         *
         * Base implementation creates and activates one world named after the project's
         * `StartupLevel` (falling back to "Main"). Override to open something else — the editor
         * will want the last-opened map rather than the game's startup level. Overriding with an
         * empty body is legal: every world consumer already handles "no active world".
         *
         * NOTE (M5): this only NAMES the world today. Loading that level's maps into it needs the
         * `.opaaxlevel` / `.opaaxmap` file layer, which does not exist yet.
         */
        virtual void CreateStartupWorld();
        
        // End Modules
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

        // Populated at RegisterModules (D9). Components() forwards to the real ComponentRegistry
        // since M3; WorldSubsystems() still counts until M4. Owned by the app so the record
        // survives boot for inspection/tests — by UniquePtr because the type is only
        // forward-declared here (see the NOTE at the top).
        UniquePtr<ModuleRegistrar> m_ModuleRegistrar;

        static AppServiceLocator m_Services;
    };
}
