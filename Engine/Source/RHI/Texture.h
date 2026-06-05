#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @interface ITexture2D
     *
     * Backend-agnostic 2D GPU texture. Consumers (Texture2D asset, FontAsset, Renderer2D)
     * hold a UniquePtr<ITexture2D> and never name a concrete backend type. The concrete
     * impl is selected by ITexture2D::Create, defined in the active backend's TU
     * (OpenGLTexture2D.cpp today) — mirrors the IVertexArray/IVertexBuffer factory pattern.
     *
     * GetRendererID exposes the raw backend handle (GL texture name) for the one consumer
     * that still needs it — the editor ViewportPanel feeding ImGui::Image. That is an
     * acknowledged backend-detail leak scoped to editor display, not the render path.
     */
    class OPAAX_API ITexture2D
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~ITexture2D() = default;

        // =============================================================================
        // Factory
        // =============================================================================
    public:
        // Load from file path (decoder lives in the backend impl).
        static UniquePtr<ITexture2D> Create(const char* InPath);

        // 1x1 solid white texture (tinted quads).
        static UniquePtr<ITexture2D> Create(Uint32 InWidth, Uint32 InHeight);

        // Raw pixel bytes. Channels: 4 = RGBA8, 3 = RGB8, 1 = R8 coverage (alpha-swizzled).
        static UniquePtr<ITexture2D> Create(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void Bind(Uint32 InSlot = 0) const = 0;
        virtual void Unbind()                const = 0;

        //------------------------------------------------------------------------------
        // Get

        virtual Uint32 GetWidth()      const noexcept = 0;
        virtual Uint32 GetHeight()     const noexcept = 0;
        virtual Uint32 GetRendererID() const noexcept = 0;
        virtual bool   IsLoaded()      const noexcept = 0;
    };
}
