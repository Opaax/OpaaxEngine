#include "UI/UIBrush.h"

namespace Opaax
{
    bool ResolveBrush(const UIBuildContext& InContext, const UIBrush& InBrush, ITexture2D* const InRuntime,
                      UIResolvedImage& Out, bool& bOutNamed)
    {
        return ResolveImageSource(InContext, InRuntime, InBrush.Texture, InBrush.Sheet, InBrush.Frame, Out, bOutNamed);
    }

    EUIBrushDraw DrawBrush(const UIBuildContext& InContext, const Bounds2D& InRect, const UIBrush& InBrush,
                           ITexture2D* const InRuntime, TDynArray<UIQuad>& OutQuads)
    {
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveBrush(InContext, InBrush, InRuntime, lImage, bNamed) && bNamed)
        {
            return EUIBrushDraw::NotReady;
        }

        if (!BuildImageQuads(InRect, lImage, InBrush.Border, InBrush.BorderScale, InBrush.Tint, OutQuads))
        {
            return EUIBrushDraw::Stretched;
        }
        return EUIBrushDraw::Drawn;
    }
}
