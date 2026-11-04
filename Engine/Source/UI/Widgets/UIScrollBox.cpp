#include "UI/Widgets/UIScrollBox.h"

#include "Core/Maths/Maths.h"
#include "Core/Reflection/OpaaxEnumJson.h"

namespace Opaax
{
    UIScrollBox::UIScrollBox()
    {
        m_bArrangesChildren = true;
        m_bClipsChildren    = true;
    }

    // =============================================================================
    // Authored state — every change moves the children, so each one invalidates the layout
    // =============================================================================

    void UIScrollBox::SetAxis(const EUIAxis InAxis)
    {
        Axis = InAxis;
        InvalidateLayout();
    }

    void UIScrollBox::SetSpacing(const float InSpacing)
    {
        Spacing = InSpacing;
        InvalidateLayout();
    }

    void UIScrollBox::SetPadding(const UIMargin& InPadding)
    {
        Padding = InPadding;
        InvalidateLayout();
    }

    void UIScrollBox::SetChildAlign(const EUIAlign InAlign)
    {
        ChildAlign = InAlign;
        InvalidateLayout();
    }

    // =============================================================================
    // Scrolling
    // =============================================================================

    float UIScrollBox::GetMaxScroll() const noexcept
    {
        const float lContent = Layout().ContentExtent(UIStackLayout::DesiredSizes(GetChildren()));
        const float lView    = Axis == EUIAxis::Vertical ? GetBounds().Size().y : GetBounds().Size().x;
        return Maths::Max(lContent - lView, 0.f);
    }

    float UIScrollBox::GetScroll() const noexcept
    {
        return Maths::Clamp(m_Scroll, 0.f, GetMaxScroll());
    }

    void UIScrollBox::SetScroll(const float InScroll)
    {
        const float lScroll = Maths::Clamp(InScroll, 0.f, GetMaxScroll());
        if (lScroll == GetScroll())
        {
            return;
        }

        m_Scroll = lScroll;
        InvalidateLayout();
    }

    // =============================================================================
    // UIWidget
    // =============================================================================

    EUIReply UIScrollBox::OnPointerEvent(const UIPointerEvent& InEvent)
    {
        switch (InEvent.Type)
        {
            case EUIPointerEventType::Scroll:
            {
                // All of it shows: the wheel is for someone else.
                if (GetMaxScroll() <= 0.f)
                {
                    return EUIReply::Unhandled;
                }

                // Up shows what comes before. A horizontal box takes a sideways wheel, else the usual one.
                const float lNotches = (Axis == EUIAxis::Horizontal && InEvent.Scroll.x != 0.f) ? InEvent.Scroll.x
                                                                                                : InEvent.Scroll.y;
                ScrollBy(-lNotches * ScrollStep);
                return EUIReply::Handled;
            }

            case EUIPointerEventType::Down:
            {
                if (!bDragToScroll || InEvent.Button != EUIPointerButton::Primary || GetMaxScroll() <= 0.f)
                {
                    return EUIReply::Unhandled;
                }

                // Handled, so the pointer is captured: its moves come here until the release.
                m_bDragging      = true;
                m_DragFrom       = InEvent.Position;
                m_DragFromScroll = GetScroll();
                return EUIReply::Handled;
            }

            case EUIPointerEventType::Move:
            {
                if (m_bDragging)
                {
                    // The content follows the pointer: moving it up (Y-up) or left scrolls on.
                    const float lMoved = Axis == EUIAxis::Vertical ? InEvent.Position.y - m_DragFrom.y
                                                                   : m_DragFrom.x - InEvent.Position.x;
                    SetScroll(m_DragFromScroll + lMoved);
                }
                return EUIReply::Unhandled;
            }

            case EUIPointerEventType::Up:
            {
                if (!m_bDragging || InEvent.Button != EUIPointerButton::Primary)
                {
                    return EUIReply::Unhandled;
                }

                m_bDragging = false;
                return EUIReply::Handled;
            }

            default:
                return EUIReply::Unhandled;
        }
    }

    void UIScrollBox::ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const
    {
        Layout().Arrange(GetBounds(), UIStackLayout::DesiredSizes(GetChildren()), GetScroll(), OutSlots);
    }
}
