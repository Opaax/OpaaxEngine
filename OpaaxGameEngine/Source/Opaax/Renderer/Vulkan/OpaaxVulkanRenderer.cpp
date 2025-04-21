#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanRenderer.h"

using namespace OPAAX::Vulkan;

OpaaxVulkanRenderer::~OpaaxVulkanRenderer()
{
    
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Init =======================")
    OPAAX_VERBOSE("======================= Renderer - Vulkan End Init =======================")
    return false;
}

void OpaaxVulkanRenderer::Resize() {}
void OpaaxVulkanRenderer::RenderFrame() {}
void OpaaxVulkanRenderer::Shutdown()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down =======================")
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down End =======================")
}
