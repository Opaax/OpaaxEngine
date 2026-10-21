#include "UI/Widgets/UIButton.h"

#include "UI/UIImageSource.h"

namespace Opaax
{
    void UIButton::SetEnabled(const bool bInEnabled)
    {
        bEnabled = bInEnabled;
        InvalidateContent();
    }

    void UIButton::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    EUIReply UIButton::OnPointerEvent(const UIPointerEvent& InEvent)
    {
        // A disabled button does not take clicks (they fall through).
        if (!bEnabled)
        {
            return EUIReply::Unhandled;
        }

        switch (InEvent.Type)
        {
            case EUIPointerEventType::Enter:
                m_bHovered = true;
                InvalidateContent();
                return EUIReply::Unhandled;   // hover is never consumed

            case EUIPointerEventType::Leave:
                m_bHovered = false;
                InvalidateContent();
                return EUIReply::Unhandled;

            case EUIPointerEventType::Down:
                if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }
                m_bPressed = true;
                InvalidateContent();
                return EUIReply::Handled;

            case EUIPointerEventType::Up:
            {
                if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }

                const bool lWasPressed = m_bPressed;
                m_bPressed = false;
                InvalidateContent();

                // A click needs the release inside; a drag-off cancels but is still consumed (we captured it).
                if (lWasPressed && GetBounds().Contains(InEvent.Position))
                {
                    OnClick.Broadcast();
                }
                return EUIReply::Handled;
            }

            default:
                return EUIReply::Unhandled;
        }
    }

    void UIButton::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        // Same rule as UIImage: named but not ready draws nothing and retries; nothing named is a plain quad.
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveImageSource(InContext, m_Texture, Texture, Sheet, Frame, lImage, bNamed) && bNamed)
        {
            InvalidateContent();
            return;
        }

        UIQuad& lQuad = OutQuads.emplace_back();
        lQuad.Bounds  = GetBounds();
        lQuad.Texture = lImage.Texture;
        lQuad.UVMin   = lImage.UVMin;
        lQuad.UVMax   = lImage.UVMax;
        lQuad.Color   = !bEnabled ? Disabled
                      : m_bPressed ? Pressed
                      : m_bHovered ? Hovered
                                   : Normal;
    }
}
