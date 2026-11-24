#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
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
    };

    // =============================================================================
    // RuntimeTexture — a texture a game makes from pixels and changes while it runs (a fog of war, a
    //   minimap). It lives in the resource manager under a runtime path ("/Runtime/Fog"), which a
    //   SpriteComponent's Texture or a UIImage names like an image file. The game keeps this handle:
    //   copies share the texture, and it is released once no handle holds it (the sprites naming it
    //   then draw nothing). Main thread only.
    // =============================================================================
    class RuntimeTexture
    {
    public:
        RuntimeTexture() = default;

        /**
         * Makes the texture and puts it under InPath; uploaded at once when there is a device.
         * @param InPath Starts with RUNTIME_MOUNT ("/Runtime/Fog"), not in use
         * @param InPixels Width * Height * Channels bytes, bottom row first. Copied.
         * @return An empty handle, with an error, when the path, the description or the pixels are wrong
         */
        static RuntimeTexture Create(ResourceManager& InResources, const OpaaxString& InPath,
                                     const RuntimeTextureDesc& InDesc, const Uint8* InPixels);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Replaces every texel. @param InPixels Width * Height * Channels bytes, bottom row first */
        bool Update(const Uint8* InPixels);

        /**
         * Replaces the texels of InRegion (X, Y from the bottom-left texel).
         * @param InPixels InRegion.Width * InRegion.Height * Channels bytes, bottom row first
         * @return False, with a warning, for a region not inside the texture or an empty handle
         */
        bool UpdateRegion(const TextureRegion& InRegion, const Uint8* InPixels);

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
