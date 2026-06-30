#include "IWindowManager.h"

#include "IConfigSystem.h"
#include "Core/Config/Config_Engine.h"
#include "Core/Application/OpaaxApplication.h"

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
    // Pure config -> props mapping.
    // =========================================================================
    WindowProps MakeWindowProps(const EngineConfigData& InData)
    {
        return WindowProps(String(InData.WindowTitle.CStr()), InData.WindowWidth, InData.WindowHeight);
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

    // =========================================================================
    // WindowManager
    // =========================================================================
    Window* WindowManager::CreateMainWindow()
    {
        if (m_Window)
        {
            OPAAX_LOG(LogWindowManager, Warn, "Main window already created");
            return m_Window.get();
        }

        const EngineConfigData& lData = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().Data();

        m_Window.reset(Window::Create(MakeWindowProps(lData)));

        OPAAX_LOG(LogWindowManager, Info, "Main window created ({}x{})", m_Window->GetWidth(), m_Window->GetHeight())

        return m_Window.get();
    }

    void WindowManager::OnShutdown()
    {
        m_Window.reset(); // ~WindowsWindow -> Shutdown(), once (RAII)
    }
}
