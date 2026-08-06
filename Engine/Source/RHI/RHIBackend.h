#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // EBackend
    // =============================================================================
    /**
     * @enum EBackend
     *
     * Graphics backend selector for the new IRHIDevice path. OpenGL is the only backend
     * today; the Vulkan enumerator is kept for future extensibility (DX12/DX11 too) but
     * currently coerces to OpenGL — the Vulkan backend is parked in Legacy/RHI/Vulkan
     * pending a new-path VulkanRHIDevice. Selection happens once, in WindowsWindow /
     * RendererManager, from config.
     *
     * Static-free: this replaces the EBackend that lived on the retired RenderAPI facade.
     */
    enum class EBackend
    {
        OpenGL,
        Vulkan
    };

    // =============================================================================
    // Backend string mapping — free functions (no static, no facade).
    // =============================================================================

    // Map a config string ("OpenGL"/"Vulkan") to EBackend. "Vulkan" -> OpenGL for now
    // (VK backend is in Legacy), unknown -> OpenGL. Both cases logged.
    EBackend    BackendFromString(const OpaaxString& InName);

    // Human-readable name for logs. Found by ADL — every engine enum spells this ToString.
    const char* ToString(EBackend InBackend) noexcept;
}
