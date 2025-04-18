#include "OPpch.h"
#include "Opaax/OpaaxApplication.h"

#include "Opaax/Window/Platform/OpaaxWindowsWindow.h"

using namespace OPAAX;

OpaaxApplication::OpaaxApplication()
{
#ifdef OPAAX_PLATFORM_WINDOWS
	m_opaaxWindow = MakeUnique<OpaaxWindowsWindow>(OpaaxWindowSpecs{});
#else
	OPAAX_ERROR("[OpaaxApplication] Unknown platform!")
	OPAAX_ASSERT(false, "Unknown platform!");
	std::runtime_error("Unknown platform!");
#endif
}

OpaaxApplication::~OpaaxApplication()
{
	
}

void OPAAX::OpaaxApplication::Run()
{
	m_opaaxWindow->Init();
	
	bIsRunning = true;
	
	while (bIsRunning)
	{
		if(m_opaaxWindow->ShouldClose())
		{
			bIsRunning = false;
			break;
		}
		
		m_opaaxWindow->OnUpdate();
	}

	bIsRunning = false;
	
	m_opaaxWindow->Shutdown();
}
