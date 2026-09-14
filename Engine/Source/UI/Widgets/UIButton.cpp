#include "UI/Widgets/UIButton.h"

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
        // A disabled button is inert: it does not take the click, so it falls through like an image.
        if (!bEnabled)
        {
            return EUIReply::Unhandled;
        }

        switch (InEvent.Type)
        {
            case EUIPointerEventType::Enter:
                m_bHovered = true;
                InvalidateContent();
                return EUIReply::Unhandled;   // hover never consumes

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

                // A click needs the release to land inside; a drag-off cancels but still consumes,
                // because this widget captured the press.
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

    void UIButton::Rebuild(const UIBuildContext& /*InContext*/, TDynArray<UIQuad>& OutQuads)
    {
        UIQuad& lQuad = OutQuads.emplace_back();
        lQuad.Bounds  = GetBounds();
        lQuad.Texture = m_Texture;
        lQuad.Color   = !bEnabled ? Disabled
                      : m_bPressed ? Pressed
                      : m_bHovered ? Hovered
                                   : Normal;
    }
}
