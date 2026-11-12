#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/Margin.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Resources/Asset/AssetRef.h"
#include "UI/UIAssetProvider.h"
#include "UI/UIQuad.h"

namespace Opaax
{
    class ITexture2D;

    // Forward-declared: an asset reference only needs the name.
    class SpriteSheet;
    class Texture2D;

    // =============================================================================
    // The image of UIImage and UIButton. A runtime pointer wins, then a sheet frame, then a texture.
    // =============================================================================

    struct UIResolvedImage
    {
        ITexture2D* Texture = nullptr;
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        Vector2F    SizePx  = { 0.f, 0.f };   // frame or texture size (for 9-slice)
        Margin      Border;                   // a sheet frame's slice border in pixels; none for a texture
    };

    /**
     * @return True with Out filled when something drawable resolved; false when nothing is named
     *   (plain colour) or it is not ready yet (bOutNamed tells them apart)
     */
    bool ResolveImageSource(const UIBuildContext& InContext, ITexture2D* InRuntime,
                                      const TAssetRef<Texture2D>& InTexture,
                                      const TAssetRef<SpriteSheet>& InSheet, Int32 InFrame,
                                      UIResolvedImage& Out, bool& bOutNamed);

    /**
     * Appends the quads that draw InImage over InRect, tinted InColor: nine for a sliced image, one
     * otherwise (one plain quad without a texture). The border is InOwnBorder when it is not all zero,
     * else the image's own (a sheet frame's); InBorderScale is canvas units per border pixel. The quads
     * already in OutQuads are left alone. UIImage and UIButton draw with it; a game's widget can too.
     */
    void BuildImageQuads(const Bounds2D& InRect, const UIResolvedImage& InImage, const Margin& InOwnBorder,
                         float InBorderScale, const LinearColor& InColor, TDynArray<UIQuad>& OutQuads);
}
