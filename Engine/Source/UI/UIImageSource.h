#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "UI/UIAssetProvider.h"

namespace Opaax
{
    class ITexture2D;

    // NAMED, never completed — a path carries its type, not its header.
    struct TextureResource;
    struct SpriteSheetResource;

    // =============================================================================
    // The image an image-shaped widget draws — ONE resolve for UIImage and UIButton (**UI25**).
    //
    //   Three ways to name it, one precedence: a RUNTIME pointer handed over by code wins
    //   (TX1's Font-over-Face), then a SHEET frame, then a TEXTURE — ResolveSpriteDraw's rule for
    //   the last two, so a sprite and a widget cannot disagree about what a sheet means.
    // =============================================================================

    struct UIResolvedImage
    {
        ITexture2D* Texture = nullptr;
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        Vector2F    SizePx  = { 0.f, 0.f };   // the frame's, or the texture's — what a 9-slice measures against
    };

    /**
     * @return true with Out filled when something drawable resolved; false when nothing is named
     *   (draw a plain colour) OR the named thing is not ready yet — `bOutNamed` tells the two
     *   apart, since only the second is worth re-arming for (UI3).
     */
    OPAAX_API bool ResolveImageSource(const UIBuildContext& InContext, ITexture2D* InRuntime,
                                      const TResourcePath<TextureResource>& InTexture,
                                      const TResourcePath<SpriteSheetResource>& InSheet, Int32 InFrame,
                                      UIResolvedImage& Out, bool& bOutNamed);
}
