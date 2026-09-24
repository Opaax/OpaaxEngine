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

    // NOTE: WindowModeFromString and ToString(EWindowMode) are GONE from here. ToString moved beside
    // its enum in Core/Window/Window.h (I11's default rule — the exception that kept it here was
    // really about the parser's logging), and the parser itself died with the string config field:
    // an EWindowMode cannot hold an unknown mode, so there is nothing to fall back from.

    // =========================================================================
    // Pure config -> props mapping.
    // =========================================================================
    WindowProps MakeWindowProps(const EngineConfigData& InData)
    {
        // No parse: the config carries an EWindowMode, so a mode that is not one of the three is not
        // representable rather than silently corrected.
        return WindowProps(InData.Window.Title, InData.Window.Width, InData.Window.Height,
                           InData.Window.Mode);
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
