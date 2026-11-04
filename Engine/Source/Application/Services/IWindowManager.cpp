#include "Application/Services/IWindowManager.h"

#include "Engine/Config/Config_Engine.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/OpaaxApplication.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullWindowManager — never has a window.
        // =====================================================================
        class NullWindowManager final : public IWindowManager
        {
        public:
            bool    IsNull()                  const noexcept override { return true; }
            Window* CreateMainWindow()                       override { return nullptr; }
            Window* GetMainWindow()           const          override { return nullptr; }
            void    FollowEngineConfig()                     override {}
            bool    IsFollowingEngineConfig() const          override { return false; }
        };
    }

    // =========================================================================
    // Config -> window props
    // =========================================================================
    WindowProps MakeWindowProps(const EngineConfigData& InData)
    {
        return WindowProps(InData.Window.Title, InData.Window.Width, InData.Window.Height,
                           InData.Window.Mode);
    }

    bool IsOtherWindowShape(const WindowProps& InShown, const WindowProps& InWanted) noexcept
    {
        return InShown.Mode != InWanted.Mode || InShown.Width != InWanted.Width || InShown.Height != InWanted.Height;
    }

    // =========================================================================
    // IWindowManager
    // =========================================================================
    TDynArray<DisplayMode> IWindowManager::GetDisplayModes() const
    {
        const Window* const lWindow = GetMainWindow();
        return (lWindow != nullptr) ? lWindow->GetDisplayModes() : TDynArray<DisplayMode>();
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID IWindowManager::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IWindowManager& IWindowManager::Null()
    {
        static NullWindowManager s_Null;
        return s_Null;
    }
}
