#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/AppServiceLocator.h"

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
        IConfigSystem&      BootConfigSystem(const IPaths& Paths);
        IProjectManager&    BootProjectManager(const IPaths& Paths);
        IJobSystem&         BootJobSystem();
        IWindowManager&     BootWindowManager();
        IEngine&            BootEngine();
        
    protected:
        /**
         * IConfigSystem::Get also register
         * But here you can Pre register config at application boot
         * @param ConfigSystem 
         */
        virtual void PreRegisterConfig(IConfigSystem& ConfigSystem);
        
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
        void CreateApplicationRenderer();
        
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
        /**
         * The app loop
         */
        void RunApplication();
        
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
        
        // Engine
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

        static AppServiceLocator m_Services;
    };
}
