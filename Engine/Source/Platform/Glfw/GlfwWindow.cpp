#include "GlfwWindow.h"

#include <GLFW/glfw3.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IWindowManager.h"
#include "Platform/Window/WindowEvents.h"
#include "Engine/Config/Config_Engine.h"

#include "Input/InputTypesFwd.hpp"

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

		// Made windowed, where the OS puts it: SetWindowMode keeps that place for when it comes back.
		m_Data.Mode			= EWindowMode::Windowed;

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

		// A null window is unrecoverable: crash.
		OPAAX_CORE_ASSERT(m_Window)

		SetWindowMode(Props.Mode, Props.Width, Props.Height);

		// Borderless and fullscreen take the screen's size; the resize callback is not there yet to say so.
		int lWidth  = 0;
		int lHeight = 0;
		glfwGetWindowSize(m_Window, &lWidth, &lHeight);
		m_Data.Width  = static_cast<Uint32>(lWidth);
		m_Data.Height = static_cast<Uint32>(lHeight);

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

	void GlfwWindow::SetWindowMode(const EWindowMode InMode, const Uint32 InWidth, const Uint32 InHeight)
	{
		if (m_Window == nullptr)
		{
			return;
		}

		// Leaving a window: where it was, to come back to.
		if (m_Data.Mode == EWindowMode::Windowed && glfwGetWindowMonitor(m_Window) == nullptr)
		{
			int lPosX = 0;
			int lPosY = 0;
			glfwGetWindowPos(m_Window, &lPosX, &lPosY);
			m_Data.PosX = lPosX;
			m_Data.PosY = lPosY;
		}

		// Found first: fullscreen, the window is on its screen; once it left, the centre decides.
		GLFWmonitor* const lScreen = FindScreen();

		// Out of fullscreen first, so the screen is back at its own mode before it is read.
		if (InMode != EWindowMode::Fullscreen && glfwGetWindowMonitor(m_Window) != nullptr)
		{
			glfwSetWindowMonitor(m_Window, nullptr, m_Data.PosX, m_Data.PosY, static_cast<int>(InWidth),
			                     static_cast<int>(InHeight), GLFW_DONT_CARE);
		}

		switch (InMode)
		{
		case EWindowMode::Windowed:
		{
			// The host's flag, not GLFW_TRUE: a window with a custom title bar is windowed and undecorated.
			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, m_Data.bDecorated ? GLFW_TRUE : GLFW_FALSE);
			glfwSetWindowMonitor(m_Window, nullptr, m_Data.PosX, m_Data.PosY, static_cast<int>(InWidth),
			                     static_cast<int>(InHeight), GLFW_DONT_CARE);
			break;
		}

		case EWindowMode::Borderless:
		{
			// A window over the whole screen, not the screen's own: no mode change, and switching to
			// another program leaves it where it is.
			const GLFWvidmode* const lDesktop = (lScreen != nullptr) ? glfwGetVideoMode(lScreen) : nullptr;
			if (lDesktop == nullptr)
			{
				return;
			}

			int lScreenX = 0;
			int lScreenY = 0;
			glfwGetMonitorPos(lScreen, &lScreenX, &lScreenY);

			glfwSetWindowAttrib(m_Window, GLFW_DECORATED, GLFW_FALSE);
			glfwSetWindowMonitor(m_Window, nullptr, lScreenX, lScreenY, lDesktop->width, lDesktop->height,
			                     GLFW_DONT_CARE);
			break;
		}

		case EWindowMode::Fullscreen:
		{
			if (lScreen == nullptr)
			{
				return;
			}

			const DisplayMode lMode = ChooseDisplayMode(ModesOf(lScreen), InWidth, InHeight);
			glfwSetWindowMonitor(m_Window, lScreen, 0, 0, static_cast<int>(lMode.Width), static_cast<int>(lMode.Height),
			                     lMode.RefreshRate > 0 ? static_cast<int>(lMode.RefreshRate) : GLFW_DONT_CARE);

			OPAAX_LOG(LogGlfwWindow, Info, "Fullscreen at {}x{}, {} Hz ({}x{} asked)", lMode.Width, lMode.Height,
			          lMode.RefreshRate, InWidth, InHeight);
			break;
		}
		}

		m_Data.Mode = InMode;
	}

	TDynArray<DisplayMode> GlfwWindow::GetDisplayModes() const
	{
		return ModesOf(FindScreen());
	}

	Uint32 GlfwWindow::GetRefreshRate() const
	{
		GLFWmonitor* const lScreen = FindScreen();
		const GLFWvidmode* lMode   = (lScreen != nullptr) ? glfwGetVideoMode(lScreen) : nullptr;
		return (lMode != nullptr && lMode->refreshRate > 0) ? static_cast<Uint32>(lMode->refreshRate) : 0u;
	}

	GLFWmonitor* GlfwWindow::FindScreen() const
	{
		if (m_Window == nullptr)
		{
			return glfwGetPrimaryMonitor();
		}

		if (GLFWmonitor* const lOwn = glfwGetWindowMonitor(m_Window))
		{
			return lOwn;
		}

		int                 lCount    = 0;
		GLFWmonitor** const lMonitors = glfwGetMonitors(&lCount);

		TDynArray<ScreenArea> lAreas;
		for (int lIndex = 0; lIndex < lCount; ++lIndex)
		{
			int lX = 0;
			int lY = 0;
			glfwGetMonitorPos(lMonitors[lIndex], &lX, &lY);

			const GLFWvidmode* const lMode = glfwGetVideoMode(lMonitors[lIndex]);
			lAreas.push_back(ScreenArea{ lX, lY, lMode != nullptr ? static_cast<Uint32>(lMode->width) : 0u,
			                             lMode != nullptr ? static_cast<Uint32>(lMode->height) : 0u });
		}

		int lPosX   = 0;
		int lPosY   = 0;
		int lWidth  = 0;
		int lHeight = 0;
		glfwGetWindowPos(m_Window, &lPosX, &lPosY);
		glfwGetWindowSize(m_Window, &lWidth, &lHeight);

		const Int32 lFound = FindScreenAt(lAreas, lPosX + lWidth / 2, lPosY + lHeight / 2);
		return lFound >= 0 ? lMonitors[lFound] : glfwGetPrimaryMonitor();
	}

	TDynArray<DisplayMode> GlfwWindow::ModesOf(GLFWmonitor* const InScreen)
	{
		TDynArray<DisplayMode> lModes;

		int                      lCount = 0;
		const GLFWvidmode* const lRaw   = (InScreen != nullptr) ? glfwGetVideoModes(InScreen, &lCount) : nullptr;
		for (int lIndex = 0; lRaw != nullptr && lIndex < lCount; ++lIndex)
		{
			lModes.push_back(DisplayMode{ static_cast<Uint32>(lRaw[lIndex].width), static_cast<Uint32>(lRaw[lIndex].height),
			                              lRaw[lIndex].refreshRate > 0 ? static_cast<Uint32>(lRaw[lIndex].refreshRate) : 0u });
		}

		return SortDisplayModes(Move(lModes));
	}

	// =============================================================================
	// Decoration
	// =============================================================================

	void GlfwWindow::SetDecorated(const bool bInDecorated)
	{
		m_Data.bDecorated = bInDecorated;

		// Borderless and fullscreen have none: the preference waits for the next windowed mode.
		if (m_Window == nullptr || m_Data.Mode != EWindowMode::Windowed) { return; }

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
