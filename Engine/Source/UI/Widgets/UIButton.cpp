#include "UI/Widgets/UIButton.h"

#include "UI/UIBrush.h"
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

    UIBrush UIButton::GetStateBrush(const EUIButtonState InState) const
    {
        UIBrush lBrush;
        switch (InState)
        {
            case EUIButtonState::Normal:   lBrush.Tint = Normal;   break;
            case EUIButtonState::Hovered:  lBrush.Tint = Hovered;  break;
            case EUIButtonState::Pressed:  lBrush.Tint = Pressed;  break;
            case EUIButtonState::Disabled: lBrush.Tint = Disabled; break;
        }
        lBrush.Texture     = Texture;
        lBrush.Sheet       = Sheet;
        lBrush.Frame       = GetStateFrame(InState);
        lBrush.Border      = Border;
        lBrush.BorderScale = BorderScale;
        return lBrush;
    }

    void UIButton::RebuildNormalState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawState(InContext, OutQuads, EUIButtonState::Normal);
    }

    void UIButton::RebuildHoveredState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawState(InContext, OutQuads, EUIButtonState::Hovered);
    }

    void UIButton::RebuildPressedState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawState(InContext, OutQuads, EUIButtonState::Pressed);
    }

    void UIButton::RebuildDisabledState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        DrawState(InContext, OutQuads, EUIButtonState::Disabled);
    }

    void UIButton::DrawState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads, const EUIButtonState InState)
    {
        // Same rule as UIImage: named but not ready draws nothing and retries; nothing named is a plain quad.
        const EUIBrushDraw lDraw = DrawBrush(InContext, GetBounds(), GetStateBrush(InState), m_Texture, OutQuads);
        if (lDraw == EUIBrushDraw::NotReady)
        {
            InvalidateContent();
            return;
        }

        if (lDraw == EUIBrushDraw::Stretched && !m_bWarnedTiles)
        {
            m_bWarnedTiles = true;
            OPAAX_LOG(LogUISlice, Warn, "'{}' would take more than {} tiles: stretched instead (raise BorderScale)",
                      Name.CStr(), UI_MAX_TILED_QUADS);
        }
    }
}
