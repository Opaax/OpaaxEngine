#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    /** How a device finds wrong graphics calls. */
    enum class ERHIErrorChecks : Uint8
    {
        None,       // not looked for (a ship build)
        PerFrame,   // the error flags are read once a frame: says that a call was wrong, not which
        PerCall     // the driver reports each wrong call as it happens
    };

    /** The name of an error check mode ("per call"). */
    constexpr const char* ToString(const ERHIErrorChecks InChecks) noexcept
    {
        switch (InChecks)
        {
            case ERHIErrorChecks::PerFrame: return "per frame";
            case ERHIErrorChecks::PerCall:  return "per call";
            default:                        return "none";
        }
    }

    // =============================================================================
    // RHICapabilities — what the running device and its driver are and can do. Read once when
    //   the device starts; the renderer sizes its requests on it (samples, texture sizes).
    // =============================================================================
    struct RHICapabilities
    {
        /** The backend's name ("OpenGL"). */
        OpaaxString Backend;

        /** As the driver names them: who made it, the GPU, the API version, the shader language version. */
        OpaaxString Vendor;
        OpaaxString Renderer;
        OpaaxString Version;
        OpaaxString ShadingLanguage;

        /** The most samples a multisampled colour target can have. 1: no multisampling. */
        Uint32 MaxSamples = 1;

        /** The longest side of a texture, in pixels. */
        Uint32 MaxTextureSize = 0;

        /** Textures a fragment shader can read in one draw. */
        Uint32 MaxTextureUnits = 0;

        ERHIErrorChecks ErrorChecks = ERHIErrorChecks::None;
    };
}
