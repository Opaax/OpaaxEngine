#pragma once

#include "IWindowManager.h"

namespace Opaax{
	// =============================================================================
    // WindowManager — owns a single Window, created on demand from Config_Engine.
    // =============================================================================
    class OPAAX_API WindowManager final : public IWindowManager
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        WindowManager()           = default;
        ~WindowManager() override = default;
        
        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        WindowManager(const WindowManager&)            = delete;
        WindowManager& operator=(const WindowManager&) = delete;
        WindowManager(WindowManager&&)                 = delete;
        WindowManager& operator=(WindowManager&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Opaax::IAppService interface
    public:
        void OnShutdown() override;
        //~End Opaax::IAppService interface

        //~Begin Opaax::IWindowManager interface
    public:
        Window* CreateMainWindow() override;
        Window* GetMainWindow() const override { return m_Window.get(); }
        //~End Opaax::IWindowManager interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<Window> m_Window;
    };
}