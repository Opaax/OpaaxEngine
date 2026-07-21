#include "VulkanContext.h"

#if OPAAX_HAS_VULKAN

#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    VulkanContext::VulkanContext(GLFWwindow* InWindow)
        : m_Window(InWindow)
    {
    }

    VulkanContext::~VulkanContext()
    {
        // Swapchain depends on the device; reset it first (the device's dtor waits idle anyway).
        m_Swapchain.reset();
        m_Device.reset();
    }

    bool VulkanContext::Init()
    {
        m_Device = MakeUnique<VulkanDevice>(m_Window);
        if (!m_Device->IsValid())
        {
            OPAAX_CORE_ERROR("VulkanContext: device bring-up failed.");
            return false;
        }

        m_Swapchain = MakeUnique<VulkanSwapchain>(*m_Device, m_Window);
        if (!m_Swapchain->IsValid())
        {
            OPAAX_CORE_ERROR("VulkanContext: swapchain bring-up failed.");
            return false;
        }

        OPAAX_CORE_INFO("VulkanContext: initialized.");
        return true;
    }

    void VulkanContext::SwapBuffers()
    {
        if (m_Swapchain) { m_Swapchain->Present(); }
    }

    void VulkanContext::SetVSync(bool /*InEnabled*/)
    {
        // Phase 2: the swapchain is built FIFO (vsync on). Toggling present mode requires a
        // swapchain recreate with a different mode — deferred to a later phase.
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
