#pragma once

#define GLFW_INCLUDE_VULKAN
#include <array>
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "OpaaxEngineMacros.h"

namespace OPAAX
{
    namespace VULKAN
    {
        inline VkResult CreateDebugUtilsMessengerEXT(VkInstance Instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                     const VkAllocationCallbacks* pAllocator,
                                                     VkDebugUtilsMessengerEXT* pDebugMessenger)
        {
            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
            if (func != nullptr)
            {
                return func(Instance, pCreateInfo, pAllocator, pDebugMessenger);
            }

            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        inline void DestroyDebugUtilsMessengerEXT(VkInstance Instance, VkDebugUtilsMessengerEXT debugMessenger,
                                                  const VkAllocationCallbacks* pAllocator)
        {
            auto lFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (lFunc != nullptr)
            {
                lFunc(Instance, debugMessenger, pAllocator);
            }
        }

        class OpaaxVulkanInstance
        {
            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            VkInstance m_vkInstance = VK_NULL_HANDLE;
            VkDebugUtilsMessengerEXT m_vkDebugMessenger = VK_NULL_HANDLE;

            bool bIsInitialized = false;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            /*---------------------------- PUBLIC ----------------------------*/
        public:
            OpaaxVulkanInstance();
            ~OpaaxVulkanInstance();

            //-----------------------------------------------------------------
            // Function
            //-----------------------------------------------------------------
            /*---------------------------- PRIVATE ----------------------------*/
        private:
            void CreateInstance();
            void SetupDebugMessenger();
            bool CheckValidationLayerSupport();
            std::vector<const char*> GetRequiredExtensions();

            void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& CreateInfo);

            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData);

            /*---------------------------- PUBLIC ----------------------------*/
        public:
            void Cleanup();

            /*---------------------------- Getter - Setter ----------------------------*/
            FORCEINLINE VkInstance GetInstance() const { return m_vkInstance; }
        };
    }
}
