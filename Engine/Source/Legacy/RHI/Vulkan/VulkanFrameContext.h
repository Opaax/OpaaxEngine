#pragma once

#include "Core/OpaaxTypes.h"

#if OPAAX_HAS_VULKAN

#include <vma/vk_mem_alloc.h>

namespace Opaax
{
    class VulkanDevice;

    // Per-frame ring depth for dynamic UBO slots + descriptor sets: the max number of distinct
    // Renderer2D Begin/Flush cycles in one frame (each needs its own UBO region + descriptor set
    // because Vulkan defers execution to submit). Target 2D games use ~2 (world + overlay); 64 is
    // generous headroom. Overflow logs + wraps (best-effort) — see the resource SetData paths.
    inline constexpr Uint32 OPAAX_VULKAN_FRAME_RING = 64;

    // =============================================================================
    // VulkanFrameContext
    // =============================================================================
    /**
     * @class VulkanFrameContext
     *
     * Controlled singleton giving Vulkan resources ambient access to the one device + the
     * current frame-in-flight slot. Resources are built by the parameterless BackendFactory and
     * write their data from neutral Renderer2D (outside the command buffer), so they cannot be
     * handed the device/frame slot through their interface — they read it here instead.
     *
     * There is genuinely one VulkanDevice + one VulkanSwapchain, so a static is the right tool
     * (mirrors Renderer2D's single static s_Data).
     *
     *   - SetDevice : called once by VulkanRenderAPI::Init.
     *   - BeginFrame: called by VulkanRenderAPI::BeginFrame on a SUCCESSFUL acquire — records the
     *     frame-in-flight slot and bumps a monotonic generation. A skipped frame does NOT bump it
     *     (no resource writes happen), so per-frame ring cursors stay put.
     *
     * Resources detect "a new frame started" by comparing their cached generation to Generation()
     * and reset their per-frame-slot ring cursors on a mismatch.
     */
    class VulkanFrameContext
    {
        // =============================================================================
        // Set (called by VulkanRenderAPI)
        // =============================================================================
    public:
        static void SetDevice(VulkanDevice* InDevice) noexcept    { s_Device = InDevice; }
        static void SetColorFormat(VkFormat InFormat) noexcept     { s_ColorFormat = InFormat; }
        static void BeginFrame(Uint32 InFrameSlot) noexcept        { s_FrameSlot = InFrameSlot; ++s_Generation; }

        // Cleared on render-API teardown so a stale device pointer never lingers.
        static void Clear() noexcept { s_Device = nullptr; }

        // =============================================================================
        // Get (read by resources)
        // =============================================================================
    public:
        static VulkanDevice* Device()      noexcept { return s_Device; }
        static VmaAllocator  Allocator()   noexcept;            // s_Device->GetAllocator()
        static VkFormat      ColorFormat() noexcept { return s_ColorFormat; }
        static Uint32        FrameSlot()   noexcept { return s_FrameSlot; }
        static Uint64        Generation()  noexcept { return s_Generation; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static VulkanDevice* s_Device;
        static VkFormat      s_ColorFormat;
        static Uint32        s_FrameSlot;
        static Uint64        s_Generation;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
