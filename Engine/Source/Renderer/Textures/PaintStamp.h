#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Resources/Asset/AssetRef.h"
#include "RHI/Pipeline.h"   // EBlendMode

namespace Opaax
{
    class Texture2D;
    class SpriteSheet;

    /** How a stamp meets what is already painted. */
    enum class EPaintMode : Uint8
    {
        /** Drawn over it (source-over). */
        Over,

        /** Takes it away, as much as the stamp's alpha (destination-out): the stamp's colour is not used. */
        Erase
    };

    /** The blend a paint pass of InMode draws with. */
    constexpr EBlendMode BlendModeOf(const EPaintMode InMode) noexcept
    {
        switch (InMode)
        {
        case EPaintMode::Over:  return EBlendMode::Paint;
        case EPaintMode::Erase: return EBlendMode::Erase;
        }
        return EBlendMode::Paint;
    }

    /**
     * One brush stamp painted into a texture (RuntimeTexture::Paint): a textured, tinted, turned quad
     * in the painted texture's texels.
     */
    struct PaintStamp
    {
        /** The brush (an image file or a /Runtime/ texture). Empty: a solid quad of Color. */
        TAssetRef<Texture2D>   Texture;

        /** A frame of a sheet instead of a texture: wins over Texture, as on a sprite. */
        TAssetRef<SpriteSheet> Sheet;
        Int32                  Frame = 0;

        /** Multiplied with the brush; straight alpha (not premultiplied). */
        Vector4F Color    = { 1.f, 1.f, 1.f, 1.f };

        /** The quad's centre, in texels from the texture's bottom-left corner. */
        Vector2F Center   = { 0.f, 0.f };

        /** Full width and height, in texels. */
        Vector2F Size     = { 1.f, 1.f };

        /** Radians, counter-clockwise, around the centre. */
        float    Rotation = 0.f;
    };
}
