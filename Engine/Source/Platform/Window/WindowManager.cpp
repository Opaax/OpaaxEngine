#include "Platform/Window/WindowManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Engine/Config/Config_Engine.h"

namespace Opaax
{
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

        const EngineConfigData& lData = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData();

        m_Shown = MakeWindowProps(lData);
        m_Window.reset(Window::Create(m_Shown));

        OPAAX_LOG(LogWindowManager, Info, "Main window created ({}x{})", m_Window->GetWidth(), m_Window->GetHeight());

        return m_Window.get();
    }

    void WindowManager::FollowEngineConfig()
    {
        if (m_EngineConfig != nullptr)
        {
            return;
        }

        m_EngineConfig = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>();
        m_EngineConfig->OnChanged().AddMember(this, &WindowManager::HandleEngineConfigChanged);
    }

    void WindowManager::HandleEngineConfigChanged()
    {
        if (m_Window == nullptr || m_EngineConfig == nullptr)
        {
            return;
        }

        // Only a new mode or size: a change elsewhere (the volume) leaves a window the player moved or
        // resized as it is.
        const WindowProps lWanted = MakeWindowProps(m_EngineConfig->GetData());
        if (!IsOtherWindowShape(m_Shown, lWanted))
        {
            return;
        }

        m_Window->SetWindowMode(lWanted.Mode, lWanted.Width, lWanted.Height);
        m_Shown = lWanted;

        OPAAX_LOG(LogWindowManager, Info, "Main window: {}, {}x{}", ToString(m_Window->GetWindowMode()),
                  m_Window->GetWidth(), m_Window->GetHeight());
    }

    void WindowManager::OnShutdown()
    {
        // The config system shuts down after this service: it is still whole here.
        if (m_EngineConfig != nullptr)
        {
            m_EngineConfig->OnChanged().RemoveAll(this);
            m_EngineConfig = nullptr;
        }

        m_Window.reset(); // destroys the window
    }
}
