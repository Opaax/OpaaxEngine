#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/TextureRegion.h"

namespace Opaax
{
    /**
     * A texture made from pixels: its size, its channels and how it is read.
     */
    struct TextureDesc
    {
        Uint32 Width    = 0;
        Uint32 Height   = 0;

        /** 4 = RGBA8, 3 = RGB8, 1 = R8 (read as (1,1,1,value), always blended). */
        Int32  Channels = 4;

        /** Read blended whatever the project's filter, without mipmaps (always so for 1 channel). */
        bool   bSmooth   = false;

        /** Its pixels can be replaced (ITexture2D::Update); counted by the device while alive. */
        bool   bWritable = false;
    };

    /**
     * 2D GPU texture. Created by IRHIDevice::CreateTexture.
     * GetRendererID gives the raw handle, for the editor (ImGui::Image) only.
     */
    class ITexture2D
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~ITexture2D() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        // Created by IRHIDevice::CreateTexture. Loading files is Texture2D's job.
    public:
        virtual void Bind(Uint32 InSlot = 0) const = 0;
        virtual void Unbind()                const = 0;

        /**
         * Replaces the pixels of InRegion (which must lie inside), in place: what draws it next shows
         * them. Only a texture made writable can.
         * @param InPixels Tightly packed rows, bottom first, of the texture's channels. Copied.
         * @return False when the texture is not writable or the region is outside
         */
        virtual bool Update(const TextureRegion& InRegion, const void* InPixels)
        {
            (void)InRegion;
            (void)InPixels;
            return false;
        }

        //------------------------------------------------------------------------------
        // Get

        virtual Uint32 GetWidth()      const noexcept = 0;
        virtual Uint32 GetHeight()     const noexcept = 0;
        virtual Uint32 GetRendererID() const noexcept = 0;
        virtual bool   IsLoaded()      const noexcept = 0;
    };
}
