#pragma once

#include "Core/EngineAPI.h"              // OPAAX_API — both functions below cross the DLL line now
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES — the backend's own value list
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
    // Backend naming + availability — free functions (no static, no facade).
    // =============================================================================

    // Human-readable name for logs, and the label this enum is WRITTEN as in a config. Found by
    // ADL — every engine enum spells this ToString (I11).
    //
    // OPAAX_API because making EngineConfigData::Render::Backend a real EBackend put this on the
    // path of EVERY TU that serializes a config — the tests and the editor exe included. It had
    // been called only from inside the DLL until then, which is I6's tell exactly.
    OPAAX_API const char* ToString(EBackend InBackend) noexcept;

    /**
     * The backend actually usable for a requested one: Vulkan -> OpenGL for now (the VK backend is
     * parked in Legacy), logged.
     *
     * This is the half of the retired BackendFromString that was never about parsing. That function
     * did two jobs under one name — config string to enum, AND "we cannot honour Vulkan yet" — and
     * only the parse died when the config field became a real EBackend. Callers ask for
     * availability, which is what they always meant.
     */
    OPAAX_API EBackend ResolveSupportedBackend(EBackend InRequested);
}

OPAAX_ENUM_VALUES(Opaax::EBackend, OpenGL, Vulkan)
