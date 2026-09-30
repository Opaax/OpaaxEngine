#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * 2D GPU texture. Created by IRHIDevice::CreateTexture.
     * GetRendererID gives the raw handle, for the editor (ImGui::Image) only.
     */
    class OPAAX_API ITexture2D
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~ITexture2D() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        // Created by IRHIDevice::CreateTexture. Loading files is TextureResource's job.
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
