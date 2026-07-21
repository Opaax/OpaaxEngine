// =============================================================================
// VulkanVMA.cpp
// =============================================================================
// The single translation unit that compiles the Vulkan Memory Allocator implementation.
// Kept separate from any consumer so VMA_IMPLEMENTATION is defined BEFORE vk_mem_alloc.h is
// first seen (a header guard would otherwise drop the implementation). VMA resolves Vulkan
// entry points against the statically-linked loader (vulkan-1) by default.

#include "Core/OpaaxTypes.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#endif // OPAAX_HAS_VULKAN
