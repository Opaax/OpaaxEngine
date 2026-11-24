#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // TextureRegion — a rectangle of texels: X, Y from the texture's first row (the bottom row, as
    //   the GPU samples it), Width and Height in texels.
    // =============================================================================
    struct TextureRegion
    {
        Uint32 X      = 0;
        Uint32 Y      = 0;
        Uint32 Width  = 0;
        Uint32 Height = 0;

        /** The whole of a InWidth x InHeight texture. */
        static TextureRegion Whole(const Uint32 InWidth, const Uint32 InHeight)
        {
            return TextureRegion{ 0u, 0u, InWidth, InHeight };
        }
    };

    /** True when InRegion is not empty and lies inside a InWidth x InHeight texture. */
    inline bool IsRegionInside(const TextureRegion& InRegion, const Uint32 InWidth, const Uint32 InHeight)
    {
        // Widened: X + Width must not wrap around.
        return InRegion.Width > 0 && InRegion.Height > 0
            && static_cast<Uint64>(InRegion.X) + InRegion.Width  <= InWidth
            && static_cast<Uint64>(InRegion.Y) + InRegion.Height <= InHeight;
    }

    /** The bytes of InRegion's pixels, tightly packed rows of InChannels bytes per texel. */
    inline Uint64 RegionByteCount(const TextureRegion& InRegion, const Int32 InChannels)
    {
        return static_cast<Uint64>(InRegion.Width) * InRegion.Height * static_cast<Uint64>(InChannels);
    }
}
