#pragma once
#include "OpaaxVKTypes.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxCoreMacros.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            /**
             * @struct OpaaxVKDescriptorLayoutBuilder
             * @brief A builder class for managing Vulkan descriptor set layouts.
             *
             * The `OpaaxVKDescriptorLayoutBuilder` class provides an abstraction for
             * creating and managing Vulkan descriptor set layouts in a streamlined fashion.
             * It allows for the addition of descriptors with various binding types and
             * configurations necessary for Vulkan descriptor set layout creation.
             *
             * This builder simplifies the descriptor layout creation process by
             * encapsulating repeated configurations and providing a clean API for
             * specifying bindings and layout properties.
             *
             * @remarks
             * This class is specifically designed for use within Vulkan-based rendering
             * or compute pipelines. It is intended to reduce boilerplate code when working
             * with descriptor set layouts.
             *
             * @note
             * Ensure that the Vulkan logical device is initialized before using this class,
             * as it typically operates in conjunction with the Vulkan API.
             *
             */
            struct OPAAX_API OpaaxVKDescriptorLayoutBuilder
            {
                VecDescSetLayoutBinding Bindings;

                /**
                 * @brief Adds a binding to the Vulkan descriptor set layout builder.
                 *
                 * The `AddBinding` method appends a new descriptor binding to the internal list of layout bindings.
                 * This binding specifies the binding index and type of descriptor required by the shader.
                 *
                 * @param Binding The binding index to associate with this descriptor in the layout.
                 * @param Type The type of Vulkan descriptor (e.g., `VK_DESCRIPTOR_TYPE_SAMPLER`, `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE`, etc.).
                 */
                void AddBinding(UInt32 Binding, VkDescriptorType Type);
                /**
                 * @brief Clears all descriptor set layout bindings from the builder.
                 *
                 * The `Clear` method removes all descriptor bindings previously added to the builder,
                 * resetting its state. This allows for reusing the builder to create a new descriptor
                 * set layout with a different configuration.
                 *
                 * @remarks
                 * This method ensures that no bindings are maintained, effectively resetting the internal
                 * vector `Bindings` to an empty state.
                 */
                void Clear();

                /**
                 * @brief Builds and creates a Vulkan descriptor set layout.
                 *
                 * The `Build` method constructs a `VkDescriptorSetLayout` using the configured bindings
                 * in the builder and specified parameters. It allows you to customize the descriptor
                 * set layout creation with shader stage flags, optional next pointer (for Vulkan extensions),
                 * and creation flags.
                 *
                 * @remarks The bindings in the builder are updated with the specified shader stages before layout creation.
                 * 
                 * @param Device The Vulkan logical device handle used for creating the descriptor set layout.
                 * @param ShaderStages A bitmask of `VkShaderStageFlags` specifying the shader stages that can access the descriptors.
                 * @param pNext A pointer to an optional Vulkan extension-specific structure, or `nullptr` if not used.
                 * @param Flags Flags of type `VkDescriptorSetLayoutCreateFlags` to modify descriptor set layout creation behavior.
                 * @return The created `VkDescriptorSetLayout` handle representing the descriptor set layout.
                 */
                VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags Flags = 0);
            };
        }
    }
}
