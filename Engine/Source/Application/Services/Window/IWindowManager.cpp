#include "IWindowManager.h"

#include "Engine/Config/Config_Engine.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/OpaaxApplication.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullWindowManager — never owns a window. Consumers check GetMainWindow().
        // =====================================================================
        class NullWindowManager final : public IWindowManager
        {
        public:
            bool    IsNull()         const noexcept override { return true; }
            Window* CreateMainWindow()              override { return nullptr; }
            Window* GetMainWindow()  const          override { return nullptr; }
        };
    }

    // =========================================================================
    // Window mode string mapping — the BackendFromString shape: unknown falls back, loudly.
    // =========================================================================
    EWindowMode WindowModeFromString(const OpaaxString& InName)
    {
        if (InName == "Windowed")   { return EWindowMode::Windowed;   }
        if (InName == "Borderless") { return EWindowMode::Borderless; }
        if (InName == "Fullscreen") { return EWindowMode::Fullscreen; }

        OPAAX_LOG(LogWindowManager, Warn, "Unknown window mode '{}' — falling back to Windowed.", InName.CStr());
        return EWindowMode::Windowed;
    }

    const char* ToString(EWindowMode InMode) noexcept
    {
        switch (InMode)
        {
            case EWindowMode::Windowed:   return "Windowed";
            case EWindowMode::Borderless: return "Borderless";
            case EWindowMode::Fullscreen: return "Fullscreen";
        }
        return "Windowed";
    }

    // =========================================================================
    // Pure config -> props mapping.
    // =========================================================================
    WindowProps MakeWindowProps(const EngineConfigData& InData)
    {
        return WindowProps(InData.Window.Title, InData.Window.Width, InData.Window.Height,
                           WindowModeFromString(InData.Window.Mode));
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
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

    // // =========================================================================
    // // WindowManager
    // // =========================================================================
    // Window* WindowManager::CreateMainWindow()
    // {
    //     if (m_Window)
    //     {
    //         OPAAX_LOG(LogWindowManager, Warn, "Main window already created");
    //         return m_Window.get();
    //     }
    //
    //     const EngineConfigData& lData = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData();
    //
    //     m_Window.reset(Window::Create(MakeWindowProps(lData)));
    //
    //     OPAAX_LOG(LogWindowManager, Info, "Main window created ({}x{})", m_Window->GetWidth(), m_Window->GetHeight())
    //
    //     return m_Window.get();
    // }
    //
    // void WindowManager::OnShutdown()
    // {
    //     m_Window.reset(); // ~WindowsWindow -> Shutdown(), once (RAII)
    // }
}
