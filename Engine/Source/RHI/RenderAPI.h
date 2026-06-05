#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "RHI/IRenderAPI.h"

namespace Opaax
{
    // =============================================================================
    // EBackend
    // =============================================================================
    /**
     * @enum EBackend
     *
     * Graphics backend selector. OpenGL + Vulkan exist today; DX12/DX11 are the intended
     * future entries. Selection happens once, in RenderSubsystem::Startup. Vulkan is always
     * a valid enumerator (the value is backend-neutral); whether it can be *built* depends on
     * OPAAX_HAS_VULKAN — the factory falls back to OpenGL when it is selected without the SDK.
     */
    enum class EBackend
    {
        OpenGL,
        Vulkan
    };

    // =============================================================================
    // RenderAPI
    // =============================================================================
    /**
     * @class RenderAPI
     *
     * Backend factory. The single place that maps EBackend -> a concrete IRenderAPI.
     * Defined in the active backend's TU (OpenGLRenderAPI.cpp). Adding a backend means
     * a new enum value + a new case here + the backend's resource Create overloads —
     * no consumer above RHI/ changes.
     */
    class OPAAX_API RenderAPI
    {
    public:
        static UniquePtr<IRenderAPI> Create(EBackend InBackend);

        // Map a config string ("OpenGL"/"Vulkan") to EBackend. Unknown -> OpenGL (logged).
        // Keeps the enum + its names in RHI so Core/Config can stay backend-agnostic.
        static EBackend             BackendFromString(const OpaaxString& InName);
        static const char*          BackendToString(EBackend InBackend) noexcept;

        // The backend chosen for this run. Set by Create (called before any resource factory),
        // so the I*::Create dispatch in BackendFactory.cpp can route to the right backend.
        static EBackend             GetBackend() noexcept { return s_Backend; }

    private:
        static EBackend s_Backend;
    };
}
