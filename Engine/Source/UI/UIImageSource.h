#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Resources/ResourcePath.h"
#include "UI/UIAssetProvider.h"

namespace Opaax
{
    class ITexture2D;

    // Forward-declared: TResourcePath only needs the name.
    struct TextureResource;
    struct SpriteSheetResource;

    // =============================================================================
    // The image of UIImage and UIButton. A runtime pointer wins, then a sheet frame, then a texture.
    // =============================================================================

    struct UIResolvedImage
    {
        ITexture2D* Texture = nullptr;
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        Vector2F    SizePx  = { 0.f, 0.f };   // frame or texture size (for 9-slice)
    };

    /**
     * @return True with Out filled when something drawable resolved; false when nothing is named
     *   (plain colour) or it is not ready yet (bOutNamed tells them apart)
     */
    bool ResolveImageSource(const UIBuildContext& InContext, ITexture2D* InRuntime,
                                      const TResourcePath<TextureResource>& InTexture,
                                      const TResourcePath<SpriteSheetResource>& InSheet, Int32 InFrame,
                                      UIResolvedImage& Out, bool& bOutNamed);
}
