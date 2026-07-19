#include "WindowManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Core/Config/Config_Engine.h"

namespace Opaax{
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
