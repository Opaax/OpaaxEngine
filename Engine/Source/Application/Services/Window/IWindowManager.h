#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Window/Window.h"

#include "Application/Services/ILogger.h"
#include "Application/Services/IAppService.h"

namespace Opaax
{
    struct EngineConfigData;

    inline constexpr LogCategory LogWindowManager{"WindowManager"};

    // Pure config -> props mapping (testable without GLFW).
    OPAAX_API WindowProps MakeWindowProps(const EngineConfigData& InData);

    // Config string <-> EWindowMode. Lives here, not in Core/Window, because Core does not log and
    // an unknown mode must be loud; the enum itself stays with the Window API.
    OPAAX_API EWindowMode  WindowModeFromString(const OpaaxString& InName);
    OPAAX_API const char* WindowModeToString(EWindowMode InMode) noexcept;

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

    // // =============================================================================
    // // WindowManager — owns a single Window, created on demand from Config_Engine.
    // // =============================================================================
    // class OPAAX_API WindowManager final : public IWindowManager
    // {
    //     // =============================================================================
    //     // CTORs - DTOR
    //     // =============================================================================
    // public:
    //     WindowManager()           = default;
    //     ~WindowManager() override = default;
    //     
    //     // =============================================================================
    //     // Copy - Move Delete
    //     // =============================================================================
    //
    //     WindowManager(const WindowManager&)            = delete;
    //     WindowManager& operator=(const WindowManager&) = delete;
    //     WindowManager(WindowManager&&)                 = delete;
    //     WindowManager& operator=(WindowManager&&)      = delete;
    //
    //     // =============================================================================
    //     // Override
    //     // =============================================================================
    //     //~Begin Opaax::IAppService interface
    // public:
    //     void OnShutdown() override;
    //     //~End Opaax::IAppService interface
    //
    //     //~Begin Opaax::IWindowManager interface
    // public:
    //     Window* CreateMainWindow() override;
    //     Window* GetMainWindow() const override { return m_Window.get(); }
    //     //~End Opaax::IWindowManager interface
    //
    //     // =============================================================================
    //     // Members
    //     // =============================================================================
    // private:
    //     UniquePtr<Window> m_Window;
    // };
}
