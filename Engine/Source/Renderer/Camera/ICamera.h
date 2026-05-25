#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    /**
     * @class ICamera
     *
     * Pure interface for cameras. Carries the matrix-producer surface and the viewport-size
     * setter — concrete subclasses (OrthographicCamera, future PerspectiveCamera) own their
     * projection model.
     *
     * Ownership: held by CameraSubsystem as UniquePtr<ICamera>. Consumers (Renderer2D,
     * RenderContext) take ICamera& by reference at the use site — never cached across
     * function calls that could trigger an active-camera swap (Lesson 17).
     */
    class OPAAX_API ICamera
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        ICamera()                              = default;
        virtual ~ICamera()                     = default;

        ICamera(const ICamera&)                = delete;
        ICamera& operator=(const ICamera&)     = delete;
        ICamera(ICamera&&)                     = delete;
        ICamera& operator=(ICamera&&)          = delete;

        // =============================================================================
        // Interface
        // =============================================================================
    public:
        /**
         * Returns the combined ViewProjection matrix. Concrete implementations may lazy-recompute.
         */
        virtual const Matrix44F& GetViewProjection() = 0;

        /**
         * Push a new viewport size into the camera. Concrete implementations update projection
         * extents accordingly.
         */
        virtual void SetViewportSize(Uint32 InWidth, Uint32 InHeight) = 0;
    };

} // namespace Opaax
