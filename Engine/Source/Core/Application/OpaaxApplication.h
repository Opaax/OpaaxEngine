#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/AppServiceLocator.h"

namespace Opaax
{
    class IPlatform;
    class IPaths;

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
    private:
        /**
         * Provide the app-level services into the locator, in dependency order.
         */
        void Bootstrap();

        /**
         * Initialize the app
         */
        void InitializeApplication();
        void CreateApplicationWindow();
        void CreateApplicationRenderer();

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        AppServiceLocator& Services() noexcept { return m_Services; }

        // Convenience accessors — never null (the locator returns the null object).
        IPlatform& Platform();
        IPaths&    Paths();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        int    m_Argc = 0;
        char** m_Argv = nullptr;

        AppServiceLocator m_Services;
    };
}
