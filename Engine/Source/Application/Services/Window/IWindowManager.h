#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Window/Window.h"

#include "Application/Services/ILogger.h"
#include "Application/Services/IAppService.h"

namespace Opaax
{
    struct EngineConfigData;

    inline constexpr LogCategory LogWindowManager{"WindowManager"};

    // Pure config -> props mapping (testable without GLFW).
    OPAAX_API WindowProps MakeWindowProps(const EngineConfigData& InData);

    // =============================================================================
    // IWindowManager — owns the application's main window. Window creation spins up a
    // GL/VK context, so it is NOT done at construction: the live host calls
    // CreateMainWindow() during InitializeApplication. Get<IWindowManager>() NEVER
    // returns null — the null object simply has no window.
    // =============================================================================
    class OPAAX_API IWindowManager : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IWindowManager)

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Create + own the main window from the engine config. Idempotent (returns the existing window on repeat calls).
         * @return 
         */
        virtual Window* CreateMainWindow() = 0;

        /**
         * @return The owned main window, or nullptr if none has been created.
         */
        virtual Window* GetMainWindow() const = 0;

        /**
         * 
         * @return true if window is not nullptr false otherwise
         */
        bool HasMainWindow() const { return GetMainWindow() != nullptr; }

        //----- null object ----------------------------------------------------
        static IWindowManager& Null();
    };
}
