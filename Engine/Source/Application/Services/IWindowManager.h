#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"

#include "Platform/Window/Window.h"

#include "Application/Services/IAppService.h"

namespace Opaax
{
    struct EngineConfigData;

    inline constexpr LogCategory LogWindowManager{"WindowManager"};

    WindowProps MakeWindowProps(const EngineConfigData& InData);

    /** True when InWanted shows a window otherwise than InShown: another mode or size. The title is read once. */
    bool IsOtherWindowShape(const WindowProps& InShown, const WindowProps& InWanted) noexcept;

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

        /**
         * From now on, the main window follows the engine config's Window.Mode, Width and Height as they
         * change: a game's does, so the player's settings show at once. The editor's does not: its
         * window is not the game's.
         */
        virtual void FollowEngineConfig() = 0;

        /** True once FollowEngineConfig was called. */
        virtual bool IsFollowingEngineConfig() const = 0;

        /** The modes the main window's screen offers in fullscreen, smallest first; none without a window. */
        TDynArray<DisplayMode> GetDisplayModes() const;

        //----- null object ----------------------------------------------------
        static IWindowManager& Null();
    };
}
