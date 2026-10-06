#include "GlfwWindow.h"

#include <GLFW/glfw3.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/Window/IWindowManager.h"
#include "Window/WindowEvents.h"
#include "Engine/Config/Config_Engine.h"

#include "Engine/Subsystems/Input/InputTypesFwd.hpp"

#include "RHI/RHIBackend.h"
#include "RHI/IGraphicsContext.h"

namespace Opaax
{
    // =============================================================================
    // Factory method
    // =============================================================================
    Opaax::Window* Opaax::Window::Create(const WindowProps& props)
    {
        return new GlfwWindow(props);
    }

    // =============================================================================
	// GlfwWindow Implementation
	// =============================================================================

	static bool s_GLFWInitialized = false;

	static void GLFWErrorCallback(int error, const char* description)
	{
		OPAAX_LOG(LogGlfwWindow, Error, "GLFW Error: {}: {}", error, description);
	}
	
	GlfwWindow::GlfwWindow(const WindowProps& Props)
	{
		Init(Props);
	}

	GlfwWindow::~GlfwWindow()
	{
		Shutdown();
	}

	void GlfwWindow::Init(const WindowProps& Props)
	{
		m_Data.Title  		= Props.Title;
		m_Data.Width  		= Props.Width;
		m_Data.Height 		= Props.Height;
		m_Data.Mode	= Props.Mode;

		if (!s_GLFWInitialized)
		{
			int bSuccess = glfwInit();
			OPAAX_CORE_ASSERT(bSuccess)
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}
		
		const EngineConfigData& lData = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData();

		// Backend from the engine config: drives window hints and context creation.
		const EBackend lBackend = ResolveSupportedBackend(lData.Render.Backend);

		// Must run before glfwCreateWindow (e.g. GLFW_NO_API for Vulkan). Nothing for OpenGL.
		IGraphicsContext::ApplyWindowHints(lBackend);

		m_Window = glfwCreateWindow(
			static_cast<int>(Props.Width),
			static_cast<int>(Props.Height),
			m_Data.Title.CStr(),
			nullptr, nullptr
		);
		
		SetWindowMode(m_Data.Mode);

		// A null window is unrecoverable: crash.
		OPAAX_CORE_ASSERT(m_Window)

		// The graphics context does make-current, glad loading and vsync.
		m_Context = IGraphicsContext::Create(lBackend, m_Window);
		OPAAX_CORE_ASSERT(m_Context)
		if (!m_Context->Init())
		{
			OPAAX_LOG(LogGlfwWindow, Error, "GlfwWindow: graphics context failed to initialize.");
		}

		// User pointer, so GLFW callbacks can reach WindowData.
		glfwSetWindowUserPointer(m_Window, &m_Data);

		RegisterGLFWCallbacks();
	}

	void GlfwWindow::RegisterGLFWCallbacks()
	{
		// ---- Window resize -------------------------------------------------------
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* InWindow, int InWidth, int InHeight)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            lData.Width  = static_cast<Uint32>(InWidth);
            lData.Height = static_cast<Uint32>(InHeight);
 
            WindowResizeEvent lEvent(lData.Width, lData.Height);
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
 
        // ---- Window close --------------------------------------------------------
        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* InWindow)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            WindowCloseEvent lEvent;
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
 
        // ---- Window focus --------------------------------------------------------
        glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* InWindow, int InFocused)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            if (InFocused)
            {
                WindowFocusEvent lEvent;
                if (lData.EventCallback) { lData.EventCallback(lEvent); }
            }
            else
            {
                WindowLostFocusEvent lEvent;
                if (lData.EventCallback) { lData.EventCallback(lEvent); }
            }
        });
 
        // ---- Window moved --------------------------------------------------------
        glfwSetWindowPosCallback(m_Window, [](GLFWwindow* InWindow, int InX, int InY)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            WindowMovedEvent lEvent(InX, InY);
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
 
        // ---- Key events ----------------------------------------------------------
        glfwSetKeyCallback(m_Window, [](GLFWwindow* InWindow, int InKey, int /*Scancode*/, int InAction, int /*Mods*/)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            if (!lData.EventCallback) { return; }
 
            const auto lKeyCode = static_cast<EKeyCode>(InKey);
 
            switch (InAction)
            {
            case GLFW_PRESS:
            {
                KeyPressedEvent lEvent(lKeyCode, false);
                lData.EventCallback(lEvent);
                break;
            }
            case GLFW_REPEAT:
            {
                KeyPressedEvent lEvent(lKeyCode, true);
                lData.EventCallback(lEvent);
                break;
            }
            case GLFW_RELEASE:
            {
                KeyReleasedEvent lEvent(lKeyCode);
                lData.EventCallback(lEvent);
                break;
            }
            default: break;
            }
        });
 
        // ---- Char / text input ---------------------------------------------------
        // For text fields and consoles, not for gameplay input.
        glfwSetCharCallback(m_Window, [](GLFWwindow* InWindow, unsigned int InCodepoint)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            KeyTypedEvent lEvent(InCodepoint);
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
 
        // ---- Mouse button --------------------------------------------------------
        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* InWindow, int InButton, int InAction, int Mods)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            if (!lData.EventCallback)
            {
	            return;
            }

        	EKeyCode lButton = EKeyCode::None;

            switch (InButton)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
            	lButton = EKeyCode::Mouse_Left;
	            break;
            case GLFW_MOUSE_BUTTON_RIGHT:
            	lButton = EKeyCode::Mouse_Right;
	            break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
            	lButton = EKeyCode::Mouse_Middle;
	            break;
            case GLFW_MOUSE_BUTTON_4:
            	lButton = EKeyCode::Mouse_Button4;
	            break;
            case GLFW_MOUSE_BUTTON_5:
            	lButton = EKeyCode::Mouse_Button5;
	            break;
            case GLFW_MOUSE_BUTTON_6:
            	lButton = EKeyCode::Mouse_Button6;
	            break;
            case GLFW_MOUSE_BUTTON_7:
            	lButton = EKeyCode::Mouse_Button7;
	            break;
            case GLFW_MOUSE_BUTTON_8:
            	lButton = EKeyCode::Mouse_Button8;
	            break;
            default: ;
            }

        	if (lButton == EKeyCode::None)
        	{
        		OPAAX_LOG(LogGlfwWindow, Error, "Receive Mouse button pressed, but no conversion to Opaax Type is found");
        		return;
        	}
        	
            switch (InAction)
            {
            case GLFW_PRESS:
            {
                MouseButtonPressedEvent lEvent(lButton);
                lData.EventCallback(lEvent);
                break;
            }
            case GLFW_RELEASE:
            {
                MouseButtonReleasedEvent lEvent(lButton);
                lData.EventCallback(lEvent);
                break;
            }
            default: break;
            }
        });
 
        // ---- Mouse scroll --------------------------------------------------------
        glfwSetScrollCallback(m_Window, [](GLFWwindow* InWindow, double InXOffset, double InYOffset)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            MouseScrolledEvent lEvent(static_cast<float>(InXOffset), static_cast<float>(InYOffset));
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
 
        // ---- Mouse position ------------------------------------------------------
        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* InWindow, double InX, double InY)
        {
            WindowData& lData = *static_cast<WindowData*>(glfwGetWindowUserPointer(InWindow));
            MouseMovedEvent lEvent(static_cast<float>(InX), static_cast<float>(InY));
            if (lData.EventCallback) { lData.EventCallback(lEvent); }
        });
    }

	void GlfwWindow::PollEvents()
    {
    	glfwPollEvents();
    }

	bool GlfwWindow::ShouldClose() const
	{
		return m_Window && glfwWindowShouldClose(m_Window);
	}

	void GlfwWindow::RequestClose()
	{
		// Sets the flag ShouldClose reads: same path as clicking the X (WindowCloseEvent fires).
		if (m_Window)
		{
			glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
		}
	}

	void GlfwWindow::SwapBuffers()
    {
	    m_Context->SwapBuffers();
    }

	void GlfwWindow::Shutdown()
	{
		if (!m_Window)
		{
			return;
		}
		
		glfwMakeContextCurrent(nullptr);
		m_Context.reset();         // before its window
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;        // cannot be destroyed twice
	}

	void GlfwWindow::SetWindowMode(EWindowMode mode)
	{
		SaveWindowedState();
		
		switch(mode)
		{
		case EWindowMode::Windowed:
			SetWindowed();
			break;

		case EWindowMode::Borderless:
			SetBorderless();
			break;

		case EWindowMode::Fullscreen:
			SetFullscreen();
			break;
		}
	}

	void GlfwWindow::SetWindowed()
	{
		// Use the host's flag, not GLFW_TRUE: a window with a custom title bar is windowed and undecorated.
		glfwSetWindowAttrib(
		m_Window,
		GLFW_DECORATED,
		m_Data.bDecorated ? GLFW_TRUE : GLFW_FALSE);

		glfwSetWindowMonitor(
			m_Window,
			nullptr,
			m_Data.PosX,
			m_Data.PosY,
			m_Data.Width,
			m_Data.Height,
			m_Data.RefreshRate);
		
		m_Data.Mode = EWindowMode::Windowed;
	}
	void GlfwWindow::SetBorderless()
	{
		GLFWmonitor* lMonitor = glfwGetPrimaryMonitor();

		if (!lMonitor)
		{
			return;
		}

		const GLFWvidmode* lVMode = glfwGetVideoMode(lMonitor);
		
		m_Data.PosX = m_Data.PosY = 0;
		m_Data.Width = lVMode->width;
		m_Data.Height = lVMode->height;
		m_Data.RefreshRate = lVMode->refreshRate;

		glfwSetWindowMonitor(
			m_Window,
			lMonitor,
			m_Data.PosX,
			m_Data.PosY,
			m_Data.Width,
			m_Data.Height,
			m_Data.RefreshRate);

		glfwSetWindowAttrib(
			m_Window,
			GLFW_DECORATED,
			GLFW_FALSE);

		glfwSetWindowPos(
			m_Window,
			m_Data.PosX,
			m_Data.PosY);
		
		m_Data.Mode = EWindowMode::Borderless;
	}
	void GlfwWindow::SetFullscreen()
	{
		GLFWmonitor* lMonitor = glfwGetPrimaryMonitor();

		if (!lMonitor)
		{
			return;
		}

		const GLFWvidmode* lVMode = glfwGetVideoMode(lMonitor);
		
		m_Data.PosX = m_Data.PosY = 0;
		m_Data.Width = lVMode->width;
		m_Data.Height = lVMode->height;
		m_Data.RefreshRate = lVMode->refreshRate;

		glfwSetWindowMonitor(
			m_Window,
			lMonitor,
			m_Data.PosX,
			m_Data.PosY,
			m_Data.Width,
			m_Data.Height,
			m_Data.RefreshRate);
		
		m_Data.Mode = EWindowMode::Fullscreen;
	}

	void GlfwWindow::SaveWindowedState()
	{
		GLFWmonitor* monitor = glfwGetWindowMonitor(m_Window);

		if (monitor != nullptr)
		{
			return;
		}

		int lPosX = 0;
		int lPosY = 0;

		glfwGetWindowPos(
			m_Window,
			&lPosX,
			&lPosY);

		m_Data.PosX = lPosX;
		m_Data.PosY = lPosY;
	}

	// =============================================================================
	// Decoration
	// =============================================================================

	void GlfwWindow::SetDecorated(const bool bInDecorated)
	{
		m_Data.bDecorated = bInDecorated;

		if (m_Window == nullptr) { return; }

		glfwSetWindowAttrib(m_Window, GLFW_DECORATED, bInDecorated ? GLFW_TRUE : GLFW_FALSE);

		// Read back from GLFW, so the log shows what actually happened.
		OPAAX_LOG(LogGlfwWindow, Trace, "Window decoration requested {} — GLFW reports {}",
		          bInDecorated ? "on" : "off", IsDecorated() ? "on" : "off");
	}

	bool GlfwWindow::IsDecorated() const
	{
		// Ask GLFW (Borderless turns decoration off without changing the preference).
		return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_DECORATED) == GLFW_TRUE;
	}

	// =============================================================================
	// Placement
	// =============================================================================

	void GlfwWindow::GetPosition(Int32& OutX, Int32& OutY) const
	{
		OutX = 0;
		OutY = 0;

		if (m_Window == nullptr) { return; }

		int lPosX = 0;
		int lPosY = 0;
		glfwGetWindowPos(m_Window, &lPosX, &lPosY);

		OutX = static_cast<Int32>(lPosX);
		OutY = static_cast<Int32>(lPosY);
	}

	void GlfwWindow::SetPosition(const Int32 InX, const Int32 InY)
	{
		if (m_Window == nullptr) { return; }

		glfwSetWindowPos(m_Window, static_cast<int>(InX), static_cast<int>(InY));
	}

	void GlfwWindow::SetSize(const Uint32 InWidth, const Uint32 InHeight)
	{
		if (m_Window == nullptr) { return; }

		glfwSetWindowSize(m_Window, static_cast<int>(InWidth), static_cast<int>(InHeight));
	}

	void GlfwWindow::Minimize()
	{
		if (m_Window != nullptr) { glfwIconifyWindow(m_Window); }
	}

	void GlfwWindow::Maximize()
	{
		if (m_Window != nullptr) { glfwMaximizeWindow(m_Window); }
	}

	void GlfwWindow::Restore()
	{
		if (m_Window != nullptr) { glfwRestoreWindow(m_Window); }
	}

	bool GlfwWindow::IsMaximized() const
	{
		return m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
	}
}
