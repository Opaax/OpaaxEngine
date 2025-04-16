#include "OpaaxEngine.h"
#include "Core/OPLogMacro.h"
#include "Renderer/OpaaxWindow.h"
#include "Renderer/Vulkan/OpaaxVulkanRenderer.h"

using namespace OPAAX;

Engine::~Engine()
{
	ShutDown();
}

Engine& Engine::Get()
{
	static Engine lInstance;
	return lInstance;
}

void Engine::Init()
{
	OPAAX_VERBOSE("Initializing Engine")
	m_pWindow = MakeUnique<OPAAX::OpaaxWindow>();

	if (!m_pWindow)
	{
		OPAAX_ERROR("Failed to create OpaaxWindow")
		throw std::runtime_error("Failed to create OpaaxWindow");
	}

	m_pWindow->InitWindow(800, 600, "OpaaxEngineWindow");

	m_pRenderer = MakeUnique<VULKAN::OpaaxVulkanRenderer>(m_pWindow.get());

	if (!m_pRenderer)
	{
		OPAAX_ERROR("Failed to create OpaaxVulkanRenderer")
		throw std::runtime_error("Failed to create OpaaxVulkanRenderer");
	}

	m_pRenderer->Initialize();
	
	OPAAX_VERBOSE("End Initializing Engine")
}

void Engine::Run()
{
	while (!m_pWindow->ShouldClose())
	{
		m_pWindow->PollEvents();
		m_pRenderer->RenderFrame();
	}
}

void Engine::ShutDown()
{
	OPAAX_VERBOSE("Start Shutting Down Engine")
	
	if(m_pRenderer)
	{
		m_pRenderer->Shutdown();
	}
	
	if (m_pWindow)
	{
		m_pWindow->Cleanup();
	}
	
	OPAAX_VERBOSE("End Shutting Down Engine")
}
