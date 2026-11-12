#include "UI/Widgets/UIButton.h"

#include "UI/UIImageSource.h"

namespace Opaax
{
    void UIButton::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    void UIButton::RebuildHoveredState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        ResolveImage(InContext, OutQuads, Hovered);
    }

    void UIButton::RebuildDisabledState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        ResolveImage(InContext, OutQuads, Disabled);
    }

    void UIButton::RebuildPressedState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        ResolveImage(InContext, OutQuads, Pressed);
    }

    void UIButton::RebuildNormalState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        ResolveImage(InContext, OutQuads, Normal);
    }

    bool UIButton::ResolveImage(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads,
        const LinearColor& InColor)
    {
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveImageSource(InContext, m_Texture, Texture, Sheet, Frame, lImage, bNamed) && bNamed)
        {
            InvalidateContent();
            return false;
        }
        
        UIQuad& lQuad = OutQuads.emplace_back();
        lQuad.Bounds  = GetBounds();
        lQuad.Texture = lImage.Texture;
        lQuad.UVMin   = lImage.UVMin;
        lQuad.UVMax   = lImage.UVMax;
        lQuad.Color   = InColor;
        
        return true;
    }
}
