#include "WindowsWindow.h"
#include <GLFW/glfw3.h>

#include "Core/ApplicationEvents.hpp"
#include "Core/Input/OpaaxInputEvents.hpp"
#include "Core/Input/OpaaxInputTypes.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/Config/EngineConfig.h"
#include "RHI/RenderAPI.h"
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
		OPAAX_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
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
		m_Data.Title  = Props.Title;
		m_Data.Width  = Props.Width;
		m_Data.Height = Props.Height;

		OPAAX_CORE_INFO("Creating window {0} ({1}, {2})", Props.Title, Props.Width, Props.Height);

		if (!s_GLFWInitialized)
		{
			int bSuccess = glfwInit();
			OPAAX_CORE_ASSERT(bSuccess)
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}

		// Backend chosen from engine config — drives window hints + context creation.
		const EBackend lBackend = RenderAPI::BackendFromString(EngineConfig::RenderBackend());

		// MUST run before glfwCreateWindow (e.g. GLFW_NO_API for Vulkan). No-op for OpenGL.
		IGraphicsContext::ApplyWindowHints(lBackend);

		m_Window = glfwCreateWindow(
			static_cast<int>(Props.Width),
			static_cast<int>(Props.Height),
			m_Data.Title.CStr(),
			nullptr, nullptr
		);

		// NOTE: Hard crash here is correct — a null window is unrecoverable.
		OPAAX_CORE_ASSERT(m_Window)

		// Graphics context owns make-current + glad load + vsync (was inline GLFW here).
		m_Context = IGraphicsContext::Create(lBackend, m_Window);
		OPAAX_CORE_ASSERT(m_Context)
		if (!m_Context->Init())
		{
			OPAAX_CORE_ERROR("WindowsWindow: graphics context failed to initialize.");
		}

		// NOTE: User pointer needed for all GLFW callbacks to reach WindowData safely.
		glfwSetWindowUserPointer(m_Window, &m_Data);

		RegisterGLFWCallbacks();

	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(m_Window);
		// Note: On ne shutdown pas GLFW ici car il pourrait y avoir d'autres fenetres
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
 
            const auto lKeyCode = static_cast<EOpaaxKeyCode>(InKey);
 
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

        	EOpaaxKeyCode lButton = EOpaaxKeyCode::None;

            switch (InButton)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
            	lButton = EOpaaxKeyCode::Mouse_Left;
	            break;
            case GLFW_MOUSE_BUTTON_RIGHT:
            	lButton = EOpaaxKeyCode::Mouse_Right;
	            break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
            	lButton = EOpaaxKeyCode::Mouse_Middle;
	            break;
            case GLFW_MOUSE_BUTTON_4:
            	lButton = EOpaaxKeyCode::Mouse_Button4;
	            break;
            case GLFW_MOUSE_BUTTON_5:
            	lButton = EOpaaxKeyCode::Mouse_Button5;
	            break;
            case GLFW_MOUSE_BUTTON_6:
            	lButton = EOpaaxKeyCode::Mouse_Button6;
	            break;
            case GLFW_MOUSE_BUTTON_7:
            	lButton = EOpaaxKeyCode::Mouse_Button7;
	            break;
            case GLFW_MOUSE_BUTTON_8:
            	lButton = EOpaaxKeyCode::Mouse_Button8;
	            break;
            default: ;
            }

        	if (lButton == EOpaaxKeyCode::None)
        	{
        		OPAAX_CORE_ERROR("Receive Mouse button pressed, but no conversion to Opaax Type is found");
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

	void WindowsWindow::SwapBuffers()
    {
	    m_Context->SwapBuffers();
    }
}
