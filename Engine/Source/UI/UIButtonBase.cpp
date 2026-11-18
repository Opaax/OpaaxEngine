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
        bIsHovered = true;
        InvalidateContent();
        OnHoveredStart.Broadcast();
        return EUIReply::Unhandled;   // hover is never consumed

    case EUIPointerEventType::Leave:
        bIsHovered = false;
        InvalidateContent();

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
        InvalidateContent();
        
        OnPressedStart.Broadcast();
        
        return EUIReply::Handled;

    case EUIPointerEventType::Up:
        {
            if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }

            const bool lWasPressed = bIsPressed;
            bIsPressed = false;
            InvalidateContent();

            // A press ends at its release; it clicks only inside. The Up is consumed either way (we captured it).
            if (lWasPressed)
            {
                OnPressedStop.Broadcast();

                if (GetBounds().Contains(InEvent.Position))
                {
                    OnClick.Broadcast();
                }
            }
            
            return EUIReply::Handled;
        }

    default:
        return UIWidget::OnPointerEvent(InEvent);
    }
}

Opaax::EUIButtonState Opaax::UIButtonBase::GetState() const noexcept
{
    if (!bIsEnabled) { return EUIButtonState::Disabled; }
    if (bIsPressed)  { return EUIButtonState::Pressed; }
    if (bIsHovered)  { return EUIButtonState::Hovered; }
    return EUIButtonState::Normal;
}

void Opaax::UIButtonBase::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
{
    switch (GetState())
    {
        case EUIButtonState::Disabled: RebuildDisabledState(InContext, OutQuads); break;
        case EUIButtonState::Pressed:  RebuildPressedState(InContext, OutQuads);  break;
        case EUIButtonState::Hovered:  RebuildHoveredState(InContext, OutQuads);  break;
        case EUIButtonState::Normal:   RebuildNormalState(InContext, OutQuads);   break;
    }
}
