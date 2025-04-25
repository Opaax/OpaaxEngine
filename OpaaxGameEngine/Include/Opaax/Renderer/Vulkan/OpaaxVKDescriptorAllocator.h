#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            /**
             * @struct OpaaxVKDescriptorAllocator
             * @brief Manages the allocation and deallocation of Vulkan descriptor sets.
             *
             * This class is responsible for pooling Vulkan descriptor sets to optimize resource usage
             * and improve performance in applications that require frequent descriptor set allocation.
             * It handles the complexities of Vulkan's descriptor set allocation to ensure efficient
             * usage of GPU resources.
             *
             * The allocator provides methods to allocate new descriptor sets, reset pools, and
             * clean up resources to prevent memory leaks and ensure proper lifecycle management
             * of Vulkan objects.
             *
             * Key Responsibilities:
             * - Pools Vulkan descriptor sets to minimize allocation overhead.
             * - Automatically handles pool creation and destruction.
             * - Prevents over-allocation of descriptors within pools.
             * - Provides mechanisms for resetting and recycling descriptor pools.
             *
             * This class should be used in scenarios where multiple Vulkan descriptor sets are
             * required for rendering or compute operations, improving both memory and performance efficiency.
             *
             * Thread Safety:
             * - This class is not thread-safe. External synchronization is required if it is used
             *   across multiple threads.
             */
            struct OPAAX_API OpaaxVKDescriptorAllocator
            {
                struct OpaaxPoolSizeRatio
                {
                    VkDescriptorType Type;
                    float Ratio;
                };

                VkDescriptorPool Pool;

                /**
                 * @brief Initializes a Vulkan descriptor pool for the allocator.
                 *
                 * This method creates a Vulkan descriptor pool with the specified maximum number of descriptor sets,
                 * utilizing a set of ratios to determine the allocation of descriptors for various descriptor types.
                 *
                 * The method configures the pool's creation parameters based on the provided ratios, ensuring that
                 * the pool is optimized for the specific needs of the application. The created pool is used internally
                 * by the allocator to manage descriptor sets.
                 *
                 * Thread Safety:
                 * - This method is not thread-safe. External synchronization is required if it is called concurrently
                 *   across multiple threads or during descriptor allocation operations.
                 *
                 * @param Device The logical Vulkan device associated with the descriptor pool.
                 * @param MaxSets The maximum number of descriptor sets that the pool can allocate.
                 * @param PoolRatios A span of descriptors and their allocation ratios used to configure the pool sizes.
                 */
                void InitPool(VkDevice Device, UInt32 MaxSets, std::span<OpaaxPoolSizeRatio> PoolRatios);
                /**
                 * @brief Resets the Vulkan descriptor pool managed by the allocator.
                 *
                 * This method clears all descriptor sets allocated from the current descriptor pool,
                 * returning the pool to its initial state. It ensures that resources are properly
                 * reset, making the pool ready for new allocations. This operation is useful when
                 * descriptor sets are no longer needed and the pool should be reused efficiently.
                 *
                 * Key Responsibilities:
                 * - Calls Vulkan to reset the descriptor pool associated with the allocator.
                 * - Frees all descriptor sets currently allocated from the pool.
                 *
                 * Thread Safety:
                 * - This method is not thread-safe. External synchronization is required if accessed
                 *   from multiple threads simultaneously.
                 *
                 * @param Device Vulkan device handle used to reset the descriptor pool. This
                 *        should correspond to the device associated with the allocator's pool.
                 */
                void ClearDescriptors(VkDevice Device);
                /**
                 * @brief Destroys the Vulkan descriptor pool managed by the allocator.
                 *
                 * This method releases the resources associated with the Vulkan descriptor pool, ensuring
                 * proper cleanup to prevent resource leaks. It calls Vulkan's `vkDestroyDescriptorPool`
                 * function to destroy the pool created during the allocation phase. After calling this,
                 * the descriptor pool will no longer be usable for descriptor allocations.
                 *
                 * Key Responsibilities:
                 * - Frees the memory and resources associated with the descriptor pool.
                 * - Ensures proper lifecycle management of Vulkan objects to avoid memory leaks.
                 *
                 * Use this method before destroying the Vulkan device or when the descriptor pool is no
                 * longer required.
                 *
                 * Thread Safety:
                 * - This method is not thread-safe. Ensure that no other threads are using the descriptor pool
                 *   while this method is being executed.
                 *
                 * @param Device Vulkan device handle used to destroy the descriptor pool. This should
                 *        be the same device associated with the allocator's pool.
                 */
                void DestroyPool(VkDevice Device);

                /**
                 * @brief Allocates a Vulkan descriptor set from the allocator's pool.
                 *
                 * This method requests a Vulkan descriptor set from the allocator, utilizing the provided
                 * device and descriptor set layout. It ensures that the allocation is made using the managed
                 * descriptor pool associated with the allocator. If the allocation succeeds, a valid
                 * VkDescriptorSet handle is returned.
                 *
                 * @param Device The Vulkan device handle used for the descriptor set allocation.
                 * @param Layout The descriptor set layout to be used for the allocation.
                 * @return A Vulkan descriptor set handle allocated from the pool. Returns a valid VkDescriptorSet
                 *         on successful allocation.
                 *
                 * @note Ensure that the allocator is properly initialized with a valid descriptor pool before
                 * calling this method. Failing to do so may result in allocation errors. The caller is
                 * responsible for managing the lifetime of the returned descriptor set in accordance with
                 * Vulkan's descriptor pool guidelines.
                 */
                VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout);
            };
        }
    }
}
