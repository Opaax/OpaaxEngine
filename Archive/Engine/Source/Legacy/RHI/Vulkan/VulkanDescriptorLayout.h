#pragma once

#include "Core/OpaaxTypes.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

namespace Opaax
{
    // The engine has exactly one sprite pipeline, so its descriptor-set shape is fixed and shared:
    //   binding 0 : sampler2D[N] (fragment)   — matches Sprite.glsl + MAX_TEXTURE_SLOTS
    //   binding 1 : camera UBO    (vertex)     — matches the binding=1 move in Sprite.glsl
    inline constexpr Uint32 OPAAX_VULKAN_SPRITE_SAMPLER_COUNT = 16;

    // Build THE canonical sprite descriptor-set layout. VulkanPipeline (for its pipeline layout)
    // and VulkanBindGroup (to allocate sets) each build their own — identically-defined layouts are
    // Vulkan-compatible, so the two stay decoupled (no shared global cache). Caller owns the handle.
    VkDescriptorSetLayout BuildSpriteDescriptorSetLayout(VkDevice InDevice);

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
