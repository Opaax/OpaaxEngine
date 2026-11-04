#pragma once

#include "Application/Services/IWindowManager.h"

namespace Opaax
{
    class Config_Engine;

	// =============================================================================
    // WindowManager — owns the main window, created from Config_Engine; in a game, it follows the
    //   config's mode and size as they change (FollowEngineConfig).
    // =============================================================================
    class WindowManager final : public IWindowManager
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
        void    FollowEngineConfig() override;
        bool    IsFollowingEngineConfig() const override { return m_EngineConfig != nullptr; }
        //~End Opaax::IWindowManager interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** The engine config changed: the window takes its mode and size when they are not the ones shown. */
        void HandleEngineConfigChanged();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<Window> m_Window;

        /** What the window shows: what it was made with, then each change followed. */
        WindowProps m_Shown;

        /** Followed while set; not owned (the config system outlives this service). */
        Config_Engine* m_EngineConfig = nullptr;
    };
}
