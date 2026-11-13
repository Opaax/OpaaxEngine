#include "UI/UIImageSource.h"

#include "RHI/Texture.h"
#include "UI/UIQuad.h"
#include "UI/UISlice.h"

namespace Opaax
{
    bool ResolveImageSource(const UIBuildContext& InContext, ITexture2D* const InRuntime,
                            const TAssetRef<Texture2D>& InTexture,
                            const TAssetRef<SpriteSheet>& InSheet, const Int32 InFrame,
                            UIResolvedImage& Out, bool& bOutNamed)
    {
        Out       = {};
        bOutNamed = InRuntime != nullptr || !InSheet.IsEmpty() || !InTexture.IsEmpty();

        if (InRuntime != nullptr)
        {
            Out.Texture = InRuntime;
            Out.SizePx  = { static_cast<float>(InRuntime->GetWidth()), static_cast<float>(InRuntime->GetHeight()) };
            return true;
        }

        if (InContext.Assets == nullptr)
        {
            return false;   // no provider, or a plain colour
        }

        if (!InSheet.IsEmpty())
        {
            const UISheetFrameView lFrame = InContext.Assets->ResolveSheetFrame(InSheet.Path.CStr(), InFrame);
            if (lFrame.Texture == nullptr)
            {
                return false;   // still uploading, or failed to load
            }

            Out.Texture = lFrame.Texture;
            Out.UVMin   = lFrame.UVMin;
            Out.UVMax   = lFrame.UVMax;
            Out.SizePx  = lFrame.SizePx;
            Out.Border    = lFrame.Border;
            Out.SliceMode = lFrame.SliceMode;
            return true;
        }

        if (!InTexture.IsEmpty())
        {
            Out.Texture = InContext.Assets->ResolveTexture(InTexture.Path.CStr());
            if (Out.Texture == nullptr)
            {
                return false;
            }

            Out.SizePx = { static_cast<float>(Out.Texture->GetWidth()), static_cast<float>(Out.Texture->GetHeight()) };
            return true;
        }

        return false;
    }

    bool BuildImageQuads(const Bounds2D& InRect, const UIResolvedImage& InImage, const Margin& InOwnBorder,
                         const float InBorderScale, const LinearColor& InColor, TDynArray<UIQuad>& OutQuads)
    {
        const Uint64  lFirst  = OutQuads.size();
        const Margin& lBorder = InOwnBorder.IsZero() ? InImage.Border : InOwnBorder;

        // A tiled frame repeats even without a border; past the cap it is drawn as if stretched.
        const bool bTile  = InImage.SliceMode == ESliceMode::Tile && InImage.Texture != nullptr;
        const bool bTiled = bTile && BuildTiledQuads(InRect, lBorder, InBorderScale, InImage.SizePx, OutQuads);

        // A border needs an image to measure against (the frame size for a sheet); else one quad.
        if (!bTiled)
        {
            if (!lBorder.IsZero() && InImage.Texture != nullptr)
            {
                BuildSlicedQuads(InRect, lBorder, InBorderScale, InImage.SizePx, OutQuads);
            }
            else
            {
                OutQuads.emplace_back().Bounds = InRect;
            }
        }

        // The slice UVs are 0..1 of the image; a frame is a sub-rect of the sheet's texture.
        const Vector2F lSpan = InImage.UVMax - InImage.UVMin;
        for (Uint64 lIndex = lFirst; lIndex < OutQuads.size(); ++lIndex)
        {
            UIQuad& lQuad = OutQuads[lIndex];
            lQuad.Color   = InColor;
            lQuad.Texture = InImage.Texture;
            lQuad.UVMin   = InImage.UVMin + lQuad.UVMin * lSpan;
            lQuad.UVMax   = InImage.UVMin + lQuad.UVMax * lSpan;
        }

        return bTiled == bTile;
    }
}
