#include "UIButtonBase.h"

#include "Renderer/Text/FontStyle.h"
#include "UI/UICanvas.h"

void Opaax::UIButtonBase::SetEnabled(bool bInEnabled)
{
    if (bIsEnabled == bInEnabled)
    {
        return;
    }

    bIsEnabled = bInEnabled;

    // A press held while disabled ends here, and the canvas lets go of it: its release does not click.
    const bool lEndsPress = !bIsEnabled && bIsPressed;
    if (lEndsPress)
    {
        bIsPressed = false;
        if (UICanvas* lCanvas = GetCanvas())
        {
            lCanvas->ReleaseCapture(*this);
        }
    }

    InvalidateContent();
    NotifyStateChanged();

    if (lEndsPress)
    {
        OnPressedStop.Broadcast();
    }

    if (bIsEnabled)
    {
        OnEnabled.Broadcast();
    }
    else
    {
        OnDisabled.Broadcast();
    }
}

Opaax::EUIReply Opaax::UIButtonBase::OnPointerEvent(const UIPointerEvent& InEvent)
{
    // A disabled button does not take clicks (they fall through).
    if (!bIsEnabled)
    {
        return EUIReply::Unhandled;
    }

    switch (InEvent.Type)
    {
    case EUIPointerEventType::Enter:
        OnHoveredStart.Broadcast();
        return EUIReply::Unhandled;   // hover is never consumed

    case EUIPointerEventType::Leave:
        // Leaving cancels the press: it ends here, and the release, wherever it is, does not click.
        if (bIsPressed)
        {
            bIsPressed = false;
            OnPressedStop.Broadcast();
        }
        
        OnHoveredStop.Broadcast();
        
        return EUIReply::Unhandled;

    case EUIPointerEventType::Down:
        if (InEvent.Button != EUIPointerButton::Primary)
        {
            return EUIReply::Unhandled;
        }
        
        bIsPressed = true;
        
        OnPressedStart.Broadcast();
        
        return EUIReply::Handled;

    case EUIPointerEventType::Up:
        {
            if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }

            const bool lWasPressed = bIsPressed;
            bIsPressed = false;

            // A press ends at its release; it clicks only inside. The Up is consumed either way (we captured it).
            if (lWasPressed)
            {
                OnPressedStop.Broadcast();

                if (GetBounds().Contains(InEvent.Position))
                {
                    NotifyActivated();   // before OnClick: a click handler may close the canvas
                    OnClick.Broadcast();
                }
            }
            
            return EUIReply::Handled;
        }

    default:
        return UIWidget::OnPointerEvent(InEvent);
    }
}
