#include "OPpch.h"
#include "Opaax/OpaaxEngine.h"

#include "Opaax/Imgui/OpaaxImguiVulkan.h"

OPAAX::OpaaxEngine::OpaaxEngine(): m_renderer() {}

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

void OPAAX::OpaaxEngine::LoadConfig()
{
    OPAAX_LOG("[OpaaxEngine] loading config")
    //TODO load base engine params
    m_renderer = RENDERER::EOPBackendRenderer::Vulkan;
}

void OPAAX::OpaaxEngine::Initialize()
{
    CreateImgui();
}

OPAAX::OpaaxEngine& OPAAX::OpaaxEngine::Get()
{
    static OpaaxEngine s_instance;
    return s_instance;
}
