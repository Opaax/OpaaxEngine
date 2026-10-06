#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

#include "Window/Window.h"

#include "Application/Services/IAppService.h"

namespace Opaax
{
    struct EngineConfigData;

    inline constexpr LogCategory LogWindowManager{"WindowManager"};
    
    WindowProps MakeWindowProps(const EngineConfigData& InData);

    // =============================================================================
    // IWindowManager — owns the application's main window.
    // =============================================================================
    class IWindowManager : public IAppService
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
         * Creates the main window from the engine config. Returns the existing one if already created.
         */
        virtual Window* CreateMainWindow() = 0;

        /**
         * @return The main window, or nullptr if not created yet
         */
        virtual Window* GetMainWindow() const = 0;

        /**
         * @return True if the main window exists
         */
        bool HasMainWindow() const { return GetMainWindow() != nullptr; }

        //----- null object ----------------------------------------------------
        static IWindowManager& Null();
    };
}
