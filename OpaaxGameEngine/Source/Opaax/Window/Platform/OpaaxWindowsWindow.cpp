#include "OPpch.h"
#include "Opaax/Window/Platform/OpaaxWindowsWindow.h"

#include "Opaax/OpaaxAssertion.h"
#include "Opaax/Log/OPLogMacro.h"

#include <SDL3/SDL.h>

#include "Opaax/OpaaxEngine.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanRenderer.h"

using namespace OPAAX;

OpaaxWindowsWindow::OpaaxWindowsWindow(const OpaaxWindowSpecs& Specs):m_window(nullptr),
	m_windowData(Specs.Title, Specs.Width, Specs.Height)
{
}

OpaaxWindowsWindow::~OpaaxWindowsWindow()
{
	if(GbIsNativeWindowInitialized)
	{
		OPAAX_WARNING("[OpaaxWindowsWindow] please use shut down before destroying the window.")
		Shutdown();
	}
}

void OpaaxWindowsWindow::InitSDLWindow()
{
	OPAAX_LOG("[OpaaxWindowsWindow], Init SDL")
	
	if(!GbIsNativeWindowInitialized)
	{
		Int32 lResult = SDL_Init(SDL_INIT_VIDEO);
		OPAAX_ASSERT(lResult, "SDL couldn't be init")
	}
#if OPAAX_DEBUG_MODE
	else
	{
		OPAAX_WARNING("[OpaaxWindowsWindow][OpaaxWindowsWindow::InitSDLWindow], SDL Already Init")
	}
#endif
}

void OpaaxWindowsWindow::Initialize()
{
	OPAAX_VERBOSE("======================= Platform - Windows Init =======================")
	OPAAX_VERBOSE("[OpaaxWindowsWindow] Creating window %1% (%2%, %3%)", %m_windowData.Title %m_windowData.Width %m_windowData.Height)

	InitSDLWindow();
	
	if(!GbIsNativeWindowInitialized)
	{
		// We initialize SDL and create a window with it.
		Int32 lResult = SDL_Init(SDL_INIT_VIDEO);
		OPAAX_ASSERT(lResult, "SDL couldn't be init")
	}

	//Since the engine can cover multiple RendererContext I need to know which flags I will pass to SDL
	SDL_WindowFlags lWindowsFlags = GetSDLWindowFlags(OpaaxEngine::Get().GetBackendRenderer());
	
	m_window = SDL_CreateWindow(
			OPAAX_CONST::ENGINE_NAME.c_str(),
			static_cast<Int32>(m_windowData.Width),
			static_cast<Int32>(m_windowData.Height),
			lWindowsFlags);

	OPAAX_LOG("[OpaaxWindowsWindow] Window created")
	
	SetVSync(true);
	
	OPAAX_VERBOSE("======================= Platform - End Windows Init =======================")
}

void OpaaxWindowsWindow::Shutdown()
{
	OPAAX_VERBOSE("======================= Platform - Windows Shutting Down =======================")
	
	SDL_DestroyWindow(m_window);
	
	GbIsNativeWindowInitialized = false;

	OPAAX_VERBOSE("======================= Platform - Windows Shutting Down End =======================")
}

bool OpaaxWindowsWindow::PollEvents(SDL_Event& Event)
{
	bool lPollResult = SDL_PollEvent(&Event);

	// Take care of this later, maybe implement some kind of wheel controller in the future?
	//case SDL_EVENT_JOYSTICK_AXIS_MOTION:
	//	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	//	case SDL_EVENT_JOYSTICK_BUTTON_UP:
	//Take care of touch later
	//	case (SDL_TouchFingerEvent):

	switch(Event.type)
	{
		case SDL_EVENT_QUIT:
			SetShouldQuit(true);
			break;
		case SDL_EVENT_WINDOW_MINIMIZED:
			SetIsMinimized(true);
			break;
		case SDL_EVENT_WINDOW_MAXIMIZED:
			SetIsMinimized(false);
			break;
		case SDL_EVENT_WINDOW_RESTORED:
			SetIsMinimized(false);
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			OpaaxEngine::Get().GetInput().RegisterKeyboardInput(Event.key);
			break;
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		case SDL_EVENT_GAMEPAD_ADDED:
		case SDL_EVENT_GAMEPAD_REMOVED:
		case SDL_EVENT_GAMEPAD_REMAPPED:
		case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
		case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
	default: ;
	}

	return lPollResult;
}

void OpaaxWindowsWindow::OnUpdate()
{
	
}

void OpaaxWindowsWindow::SetVSync(bool Enabled)
{
	if (Enabled)
	{
		
	}
	else
	{
		
	}

	m_windowData.bVSync = Enabled;
}