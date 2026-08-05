#include "WindowsWindow.h"

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/Window/IWindowManager.h"   // WindowModeToString
#include "Core/Window/WindowEvents.h"
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
        return new WindowsWindow(props);
    }

    // =============================================================================
	// WindowsWindow Implementation
	// =============================================================================

	static bool s_GLFWInitialized = false;

	static void GLFWErrorCallback(int error, const char* description)
	{
		OPAAX_LOG(LogWindowsWindow, Error, "GLFW Error: {}: {}", error, description)
	}
	
	WindowsWindow::WindowsWindow(const WindowProps& Props)
	{
		Init(Props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& Props)
	{
		m_Data.Title  		= Props.Title;
		m_Data.Width  		= Props.Width;
		m_Data.Height 		= Props.Height;
		m_Data.Mode	= Props.Mode;

		OPAAX_LOG(LogWindowsWindow, Info, "Creating window {} ({}, {}) [{}]", Props.Title, Props.Width, Props.Height,
			WindowModeToString(Props.Mode))

		if (!s_GLFWInitialized)
		{
			int bSuccess = glfwInit();
			OPAAX_CORE_ASSERT(bSuccess)
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}
		
		const EngineConfigData& lData = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().Data();

		// Backend chosen from engine config — drives window hints + context creation.
		const EBackend lBackend = BackendFromString(lData.RenderBackend);

		// MUST run before glfwCreateWindow (e.g. GLFW_NO_API for Vulkan). No-op for OpenGL.
		IGraphicsContext::ApplyWindowHints(lBackend);

		m_Window = glfwCreateWindow(
			static_cast<int>(Props.Width),
			static_cast<int>(Props.Height),
			m_Data.Title.CStr(),
			nullptr, nullptr
		);
		
		SetWindowMode(m_Data.Mode);

		// NOTE: Hard crash here is correct — a null window is unrecoverable.
		OPAAX_CORE_ASSERT(m_Window)

		// Graphics context owns make-current + glad load + vsync (was inline GLFW here).
		m_Context = IGraphicsContext::Create(lBackend, m_Window);
		OPAAX_CORE_ASSERT(m_Context)
		if (!m_Context->Init())
		{
			OPAAX_LOG(LogWindowsWindow, Error, "WindowsWindow: graphics context failed to initialize.")
		}

		// NOTE: User pointer needed for all GLFW callbacks to reach WindowData safely.
		glfwSetWindowUserPointer(m_Window, &m_Data);

		RegisterGLFWCallbacks();
	}

	void WindowsWindow::RegisterGLFWCallbacks()
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
        //Use this for text fields, debug console — NOT for gameplay input.
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
        		OPAAX_LOG(LogWindowsWindow, Error, "Receive Mouse button pressed, but no conversion to Opaax Type is found")
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

	void WindowsWindow::PollEvents()
    {
    	glfwPollEvents();
    }

	bool WindowsWindow::ShouldClose() const
	{
		return m_Window && glfwWindowShouldClose(m_Window);
	}

	void WindowsWindow::RequestClose()
	{
		// Sets the very flag ShouldClose reads, so the loop stops and WindowCloseEvent fires
		// exactly as it does for a real click on the X — one close path, not two.
		if (m_Window)
		{
			glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
		}
	}

	void WindowsWindow::SwapBuffers()
    {
	    m_Context->SwapBuffers();
    }

	void WindowsWindow::Shutdown()
	{
		if (!m_Window)
		{
			return;
		}
		
		glfwMakeContextCurrent(nullptr);
		m_Context.reset();         // release the graphics context before its window
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;        // can't be destroyed twice
	}

	void WindowsWindow::SetWindowMode(EWindowMode mode)
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

	void WindowsWindow::SetWindowed()
	{
		glfwSetWindowAttrib(
		m_Window,
		GLFW_DECORATED,
		GLFW_TRUE);

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
	void WindowsWindow::SetBorderless()
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
	void WindowsWindow::SetFullscreen()
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

	void WindowsWindow::SaveWindowedState()
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
}
