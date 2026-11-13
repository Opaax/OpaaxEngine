#include "UI/Widgets/UIButton.h"

#include "UI/UIImageSource.h"
#include "UI/UISlice.h"

namespace Opaax
{
    void UIButton::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    Int32 UIButton::GetStateFrame(const EUIButtonState InState) const noexcept
    {
        Int32 lFrame = -1;
        switch (InState)
        {
            case EUIButtonState::Hovered:  lFrame = HoveredFrame;  break;
            case EUIButtonState::Pressed:  lFrame = PressedFrame;  break;
            case EUIButtonState::Disabled: lFrame = DisabledFrame; break;
            case EUIButtonState::Normal:   break;
        }
        return lFrame >= 0 ? lFrame : Frame;
    }

    void UIButton::RebuildNormalState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawImage(InContext, OutQuads, GetStateFrame(EUIButtonState::Normal), Normal);
    }

    void UIButton::RebuildHoveredState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawImage(InContext, OutQuads, GetStateFrame(EUIButtonState::Hovered), Hovered);
    }

    void UIButton::RebuildPressedState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawImage(InContext, OutQuads, GetStateFrame(EUIButtonState::Pressed), Pressed);
    }

    void UIButton::RebuildDisabledState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawImage(InContext, OutQuads, GetStateFrame(EUIButtonState::Disabled), Disabled);
    }

    bool UIButton::DrawImage(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads, const Int32 InFrame,
                             const LinearColor& InColor)
    {
        // Same rule as UIImage: named but not ready draws nothing and retries; nothing named is a plain quad.
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveImageSource(InContext, m_Texture, Texture, Sheet, InFrame, lImage, bNamed) && bNamed)
        {
            InvalidateContent();
            return false;
        }

        if (!BuildImageQuads(GetBounds(), lImage, Border, BorderScale, InColor, OutQuads) && !m_bWarnedTiles)
        {
            m_bWarnedTiles = true;
            OPAAX_LOG(LogUISlice, Warn, "'{}' would take more than {} tiles: stretched instead (raise BorderScale)",
                      Name.CStr(), UI_MAX_TILED_QUADS);
        }
        return true;
    }
}
