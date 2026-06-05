#pragma once

#include "RHI/IGraphicsContext.h"

#if OPAAX_HAS_VULKAN

#include "Core/OpaaxTypes.h"

struct GLFWwindow;

namespace Opaax
{
    class VulkanDevice;
    class VulkanSwapchain;

    // =============================================================================
    // VulkanContext
    // =============================================================================
    /**
     * @class VulkanContext
     *
     * IGraphicsContext for Vulkan. Owns the VulkanDevice + VulkanSwapchain (built in Init,
     * after the GLFW window exists with GLFW_NO_API). Present lives here (SwapBuffers); the
     * VulkanRenderAPI borrows GetDevice()/GetSwapchain() to acquire + submit.
     */
    class VulkanContext final : public IGraphicsContext
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanContext(GLFWwindow* InWindow);
        ~VulkanContext() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IGraphicsContext interface
    public:
        bool Init()                   override;
        void SwapBuffers()            override;
        void SetVSync(bool InEnabled) override;
        //~End IGraphicsContext interface

        // =============================================================================
        // Get — borrowed by VulkanRenderAPI
        // =============================================================================
    public:
        VulkanDevice&    GetDevice()    noexcept { return *m_Device; }
        VulkanSwapchain& GetSwapchain() noexcept { return *m_Swapchain; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow*                m_Window = nullptr;
        UniquePtr<VulkanDevice>    m_Device;       // declared first -> destroyed last
        UniquePtr<VulkanSwapchain> m_Swapchain;    // destroyed before the device
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
