#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Renderer/Textures/PaintStamp.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"
#include "RHI/TextureRegion.h"

namespace Opaax
{
    class Texture2D;

    /**
     * What a texture made in code is: its size in texels, its channels and how it is read.
     */
    struct RuntimeTextureDesc
    {
        Uint32 Width    = 0;
        Uint32 Height   = 0;

        /** 4 = RGBA8, 1 = R8 (read as (1,1,1,value), always blended: a mask, a fog). */
        Int32  Channels = 4;

        /** A 4-channel texture read blended whatever the project's filter (Pixel gives nearest otherwise). */
        bool   bSmooth  = false;

        /**
         * Painted on the GPU (Paint, Clear) instead of changed by pixels: 4 channels, read blended
         * through its mipmaps whatever the filter, the pixels optional (transparent without them).
         */
        bool   bPaintable = false;
    };

    // =============================================================================
    // RuntimeTexture — a texture a game makes from pixels and changes while it runs (a fog of war, a
    //   minimap). It lives in the resource manager under a runtime path ("/Runtime/Fog"), which a
    //   SpriteComponent's Texture or a UIImage names like an image file. The game keeps this handle:
    //   copies share the texture, and it is released once no handle holds it (the sprites naming it
    //   then draw nothing). Main thread only. A paintable one (bPaintable) is painted on the GPU
    //   instead: Paint and Clear are drawn into it before the frame's views, so the sprites naming it
    //   show them the same frame.
    // =============================================================================
    class RuntimeTexture
    {
    public:
        RuntimeTexture() = default;

        /**
         * Makes the texture and puts it under InPath; uploaded at once when there is a device.
         * @param InPath Starts with RUNTIME_MOUNT ("/Runtime/Fog"), not in use
         * @param InPixels Width * Height * Channels bytes, bottom row first. Copied. Optional for a
         *   paintable texture (straight alpha; transparent when null).
         * @return An empty handle, with an error, when the path, the description or the pixels are wrong
         */
        static RuntimeTexture Create(ResourceManager& InResources, const OpaaxString& InPath,
                                     const RuntimeTextureDesc& InDesc, const Uint8* InPixels);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Replaces every texel (not of a paintable texture). @param InPixels Width * Height * Channels bytes, bottom row first */
        bool Update(const Uint8* InPixels);

        /**
         * Replaces the texels of InRegion (X, Y from the bottom-left texel).
         * @param InPixels InRegion.Width * InRegion.Height * Channels bytes, bottom row first
         * @return False, with a warning, for a region not inside the texture, a paintable texture or an empty handle
         */
        bool UpdateRegion(const TextureRegion& InRegion, const Uint8* InPixels);

        /**
         * Paints InStamps into a paintable texture over what it holds, in order, before this frame's
         * views. Stamps that draw nothing (a size not above 0, a value not a number) are dropped with a warning.
         * @return False, with a warning, for an empty handle or a texture not made paintable
         */
        bool Paint(const TDynArray<PaintStamp>& InStamps, EPaintMode InMode = EPaintMode::Over);

        /** Sets every texel of a paintable texture to InColor (straight alpha), before this frame's views. */
        bool Clear(const Vector4F& InColor = Vector4F{ 0.f, 0.f, 0.f, 0.f });

        /**
         * Sets the texels of InRegion (X, Y from the bottom-left texel) to InColor, the part outside the
         * texture left out.
         * @return False, with a warning, for a region wholly outside, an empty handle or a texture not paintable
         */
        bool ClearRegion(const TextureRegion& InRegion, const Vector4F& InColor = Vector4F{ 0.f, 0.f, 0.f, 0.f });

        /** Lets go of the texture: released at the next resource update once no handle holds it. */
        void Reset();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool IsValid() const;

        /** The runtime path sprites and images name it by. */
        const OpaaxString& GetPath() const noexcept { return m_Path; }

        Uint32 GetWidth()    const;
        Uint32 GetHeight()   const;
        Int32  GetChannels() const;

        /** True for a texture made with bPaintable. */
        bool   IsPaintable() const;

        /** The texture, or null for an empty handle. */
        Texture2D* GetTexture() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceRef<AssetResource> m_Ref;
        OpaaxString                m_Path;
    };
}
