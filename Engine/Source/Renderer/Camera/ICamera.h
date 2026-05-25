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

        //------------------------------------------------------------------------------
        // Position — defaulted so cameras without a positional concept (HUD, locked overlay)
        // can ignore. Controllers (Follow, Shake) drive these on the active camera.

        /**
         * Returns the camera's world-space position. Defaults to origin for cameras
         * with no positional concept.
         */
        virtual Vector2F GetPosition() const { return Vector2F(0.f, 0.f); }

        /**
         * Set the camera's base position. Default is no-op.
         */
        virtual void SetPosition(const Vector2F& /*InPosition*/) {}

        /**
         * Add an offset to the camera's position. Default routes through GetPosition + SetPosition;
         * concretes that distinguish base from transient offsets (e.g. for shake) override.
         */
        virtual void AddPositionOffset(const Vector2F& InOffset)
        {
            SetPosition(GetPosition() + InOffset);
        }
    };

} // namespace Opaax
