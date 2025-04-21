#include "OPpch.h"
#include "Opaax/OpaaxApplication.h"

#include "Opaax/OpaaxAssertion.h"
#include "Opaax/OpaaxEngine.h"
#include "Opaax/Window/Platform/OpaaxWindowsWindow.h"

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

void OpaaxApplication::CreateInitRenderer()
{
	OPAAX_LOG("[OpaaxApplication] Creating Renderer...")
	switch (OpaaxEngine::Get().GetRenderer())
	{
	case RENDERER::EOPBackendRenderer::Unknown:
		break;
	case RENDERER::EOPBackendRenderer::Vulkan:
		m_opaaxRenderer = MakeUnique<Vulkan::OpaaxVulkanRenderer>(m_opaaxWindow.get());
		break;
	case RENDERER::EOPBackendRenderer::Dx12:
		break;
	case RENDERER::EOPBackendRenderer::Metal:
		break;
	default: ;
	}

	OPAAX_ASSERT(m_opaaxRenderer != nullptr, "Renderer not created")
	
	m_opaaxRenderer->Initialize();
}

void OpaaxApplication::Initialize()
{
	OPAAX_VERBOSE("======================= Application Initialize =======================")
	
	OpaaxEngine::Get().LoadConfig();
	
	CreateInitMainWindow();
	CreateInitRenderer();

	bIsInitialize = true;
	OPAAX_VERBOSE("======================= Application End Initialize =======================")
}

void OPAAX::OpaaxApplication::Run()
{
	bIsRunning = true;
	
	while (bIsRunning)
	{
		m_opaaxWindow->PollEvents();
		
		if(m_opaaxWindow->ShouldClose())
		{
			bIsRunning = false;
			break;
		}
		
		m_opaaxWindow->OnUpdate();
	}
	
	Shutdown();
}

void OpaaxApplication::Shutdown()
{
	OPAAX_VERBOSE("======================= Application Shutting down =======================")
	
	bIsRunning = false;
	
	m_opaaxRenderer->Shutdown();
	m_opaaxWindow->Shutdown();
	
	OPAAX_VERBOSE("======================= Application Shutting down End =======================")
}
