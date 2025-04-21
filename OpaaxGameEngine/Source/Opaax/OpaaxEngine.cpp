#include "OPpch.h"
#include "Opaax/OpaaxEngine.h"

OPAAX::OpaaxEngine::OpaaxEngine(): m_renderer() {}

void OPAAX::OpaaxEngine::LoadConfig()
{
    OPAAX_LOG("[OpaaxEngine] loading config")
    //TODO load base engine params
    m_renderer = RENDERER::EOPBackendRenderer::Vulkan;
}

void OPAAX::OpaaxEngine::Init()
{
}

OPAAX::OpaaxEngine& OPAAX::OpaaxEngine::Get()
{
    static OpaaxEngine s_instance;
    return s_instance;
}
