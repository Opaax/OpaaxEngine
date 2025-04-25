#include "OPpch.h"
#include "Opaax/OpaaxApplication.h"

#include "Opaax/OpaaxAssertion.h"
#include "Opaax/OpaaxEngine.h"
#include "Opaax/Window/Platform/OpaaxWindowsWindow.h"

//IMGUi
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

using namespace OPAAX;

OpaaxApplication::OpaaxApplication()
{
	
}

OpaaxApplication::~OpaaxApplication()
{
	
}

void OpaaxApplication::CreateInitMainWindow()
{
	OPAAX_LOG("[OpaaxApplication] Creating main window...")
#ifdef OPAAX_PLATFORM_WINDOWS
	m_opaaxWindow = MakeUnique<OpaaxWindowsWindow>(OpaaxWindowSpecs{});
#else
	OPAAX_ERROR("[OpaaxApplication] Unknown platform!")
	OPAAX_ASSERT(false, "Unknown platform!");
	std::runtime_error("Unknown platform!");
#endif

	OPAAX_ASSERT(m_opaaxWindow != nullptr, "Main window not created")
	m_opaaxWindow->Initialize();
}

void OpaaxApplication::Initialize()
{
	OPAAX_VERBOSE("======================= Application Initialize =======================")

	//Loading base config for the engine
	OpaaxEngine::Get().LoadConfig();
	CreateInitMainWindow();
	OpaaxEngine::Get().Initialize(*m_opaaxWindow);
	
	bIsInitialize = true;
	
	OPAAX_VERBOSE("======================= Application End Initialize =======================")
}

void OpaaxApplication::Run()
{
	bIsRunning = true;
	
	while (bIsRunning)
	{
		// Handle events on queue
		SDL_Event lEvent;
		while (m_opaaxWindow->PollEvents(lEvent))
		{
			OpaaxEngine::Get().PollEvents(lEvent);
		}

		//The app will close
		if(m_opaaxWindow->ShouldClose())
		{
			//TODO Close event to make some save or other stuff
			bIsRunning = false;
			break;
		}

		if(m_opaaxWindow->IsMinimized())
		{
			//TODO Stop rendering
			//TODO Make a save?
			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		OpaaxEngine::Get().PreUpdate();
		OpaaxEngine::Get().Update();
		OpaaxEngine::Get().PostUpdate();
	}
	
	Shutdown();
}

void OpaaxApplication::Shutdown()
{
	OPAAX_VERBOSE("======================= Application Shutting down =======================")
	
	bIsRunning = false;
	
	OpaaxEngine::Get().Shutdown();
	m_opaaxWindow->Shutdown();
	
	OPAAX_VERBOSE("======================= Application Shutting down End =======================")
}
