#pragma once
#include "OpaaxEngineMacros.h"
#include "OpaaxVulkanTypes.h"

namespace OPAAX
{
    class OpaaxWindow;
    
    namespace VULKAN
    {
        class OpaaxVulkanSwapChain
        {
            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
            private:
            OpaaxWindow*                m_window{};
            VkDevice                    m_logicalDevice = VK_NULL_HANDLE; //Do not manage memory here.
            VkSwapchainKHR              m_vkSwapChain   = VK_NULL_HANDLE;
            
            std::vector<VkImage>        m_vkSwapChainImages;
            VkFormat                    m_vkSwapChainImageFormat = VK_FORMAT_UNDEFINED;
            VkExtent2D                  m_vkSwapChainExtent{};
            VecVkImgView                m_vkSwapChainImageViews;

            bool bIsInitialized = false;

            //-----------------------------------------------------------------
            // CTOR / DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            OpaaxVulkanSwapChain() = default;
            OpaaxVulkanSwapChain(OpaaxWindow* Window, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice);
            ~OpaaxVulkanSwapChain();
            
            //-----------------------------------------------------------------
            // Function
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            void CreateSwapChain(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice);
            void CreateImageViews(VkDevice LogicalDevice);
            
            VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats);
            VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes);
            VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities);
            
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            void Cleanup(VkDevice LogicalDevice);
            void CleanupForRecreate(VkDevice LogicalDevice);
            void Recreate(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice);

            /*---------------------------- Getter - Setter ----------------------------*/
            FORCEINLINE VkFormat            GetSwapChainImageFormat()   const   { return m_vkSwapChainImageFormat; }
            FORCEINLINE const VecVkImgView& GetSwapChainImageViews()            { return m_vkSwapChainImageViews; }
            FORCEINLINE VkExtent2D          GetSwapChainExtent()        const   { return m_vkSwapChainExtent; }
            FORCEINLINE VkSwapchainKHR      GetSwapChain()              const   { return m_vkSwapChain; }
        };
    }
}
