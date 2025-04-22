#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanRenderer.h"
#include "Opaax/Window/OpaaxWindow.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

using namespace OPAAX::RENDERER::VULKAN;

void OpaaxVulkanRenderer::CreateVulkanSurface()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Creating the vulkan surface...")
    
    SDL_Vulkan_CreateSurface(reinterpret_cast<SDL_Window*>(GetOpaaxWindow()->GetNativeWindow()), m_opaaxVKInstance->GetInstance(), nullptr, &m_vkSurface);

    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan surface created!")
}

OpaaxVulkanRenderer::~OpaaxVulkanRenderer()
{
    IOpaaxRendererContext::~IOpaaxRendererContext();
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Init =======================")

    //Instance
    {
        m_opaaxVKInstance = MakeUnique<OpaaxVKInstance>();
        m_opaaxVKInstance->Init();
    }

    //SURFACE
    {
        CreateVulkanSurface();
    }

    //PHYSICAL DEVICE
    {
        m_opaaxVKPhysicalDevice = MakeUnique<OpaaxVKPhysicalDevice>();
        m_opaaxVKPhysicalDevice->Init(m_opaaxVKInstance->GetInstance(), m_vkSurface);
    }
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan End Init =======================")
    return false;
}

void OpaaxVulkanRenderer::Resize() {}
void OpaaxVulkanRenderer::RenderFrame() {}
void OpaaxVulkanRenderer::Shutdown()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down =======================")

    m_opaaxVKPhysicalDevice->Cleanup();
    vkDestroySurfaceKHR(m_opaaxVKInstance->GetInstance(), m_vkSurface, nullptr);
    m_opaaxVKInstance->Cleanup();
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down End =======================")
}
