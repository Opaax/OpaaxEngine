#include "OPpch.h"
#include "Opaax/OpaaxEngine.h"

#include "Opaax/OpaaxAssertion.h"
#include "Opaax/Imgui/OpaaxImguiVulkan.h"

OPAAX::OpaaxEngine::OpaaxEngine(): m_renderer()
{
}

void OPAAX::OpaaxEngine::CreateImgui()
{
    switch (m_renderer)
    {
    case RENDERER::EOPBackendRenderer::Unknown:
        break;
    case RENDERER::EOPBackendRenderer::Vulkan:
        m_imgui = MakeUnique<IMGUI::OpaaxImguiVulkan>();
        break;
    case RENDERER::EOPBackendRenderer::Dx12:
        break;
    case RENDERER::EOPBackendRenderer::Metal:
        break;
    }
}

void OPAAX::OpaaxEngine::CreateRenderer()
{
    OPAAX_LOG("[OpaaxApplication] Creating Renderer...")
    
    switch (m_renderer)
    {
    case RENDERER::EOPBackendRenderer::Unknown:
        break;
    case RENDERER::EOPBackendRenderer::Vulkan:
        m_opaaxRenderer = MakeUnique<RENDERER::VULKAN::OpaaxVulkanRenderer>();
        break;
    case RENDERER::EOPBackendRenderer::Dx12:
        break;
    case RENDERER::EOPBackendRenderer::Metal:
        break;
    default: ;
    }

    OPAAX_ASSERT(m_opaaxRenderer != nullptr, "Renderer not created")
    OPAAX_LOG("[OpaaxApplication] Renderer %1% Created!", %typeid(m_opaaxRenderer).name())
}

void OPAAX::OpaaxEngine::LoadConfig()
{
    OPAAX_LOG("[OpaaxEngine] loading config")
    //TODO load base engine params
    m_renderer = RENDERER::EOPBackendRenderer::Vulkan;
}

void OPAAX::OpaaxEngine::Initialize(OpaaxWindow& OpaaxWindow)
{
    m_opaaxWindow = &OpaaxWindow;

    /* ------------- CREATE Engine Systems ---------------*/
    CreateImgui();
    CreateRenderer();

    /* ------------- Init Engine Systems ---------------*/
    m_opaaxRenderer->Initialize(OpaaxWindow);
}

void OPAAX::OpaaxEngine::PollEvents(SDL_Event& Event)
{
    m_imgui->PollEvents(Event);
}

void OPAAX::OpaaxEngine::PreUpdate()
{
    m_imgui->PreUpdate();
}

void OPAAX::OpaaxEngine::Update()
{
    m_opaaxRenderer->DrawImgui();
}

void OPAAX::OpaaxEngine::PostUpdate()
{
    m_imgui->PostUpdate();
    m_opaaxRenderer->RenderFrame();
}

void OPAAX::OpaaxEngine::Shutdown()
{
    m_opaaxRenderer->Shutdown();
}

OPAAX::OpaaxEngine& OPAAX::OpaaxEngine::Get()
{
    static OpaaxEngine s_instance;
    return s_instance;
}
