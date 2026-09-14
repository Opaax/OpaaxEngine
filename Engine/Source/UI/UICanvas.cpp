#include "UI/UICanvas.h"

#include <limits>

#include "Application/Services/ILogger.h"
#include "Renderer/Renderer2D.h"
#include "UI/Widgets/UIPanel.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UI);

    namespace
    {
        constexpr Int32 MAX_ORDER = std::numeric_limits<Int16>::max();

        /** Tree order, parents before children, so OrderInLayer reproduces it through F5's sort. */
        void SubmitTree(const UIWidget& InWidget, Renderer2D& InRenderer, Int32& InOutOrder)
        {
            if (!InWidget.bVisible)
            {
                return;
            }

            for (const UIQuad& lQuad : InWidget.GetQuads())
            {
                const Int16 lOrder = static_cast<Int16>(InOutOrder < MAX_ORDER ? InOutOrder++ : MAX_ORDER);

                if (lQuad.Outline > 0.f)
                {
                    InRenderer.DrawQuadOutline(lQuad.Bounds.Center, lQuad.Bounds.Size(), lQuad.Color, lQuad.Outline,
                                               0.f, ERenderLayer::UI, lOrder);
                }
                else if (lQuad.Texture != nullptr)
                {
                    InRenderer.DrawSprite(lQuad.Bounds.Center, lQuad.Bounds.Size(), *lQuad.Texture, lQuad.Color,
                                          0.f, ERenderLayer::UI, lOrder, lQuad.UVMin, lQuad.UVMax);
                }
                else
                {
                    InRenderer.DrawQuad(lQuad.Bounds.Center, lQuad.Bounds.Size(), lQuad.Color,
                                        0.f, ERenderLayer::UI, lOrder);
                }
            }

            for (const TUniquePtr<UIWidget>& lChild : InWidget.GetChildren())
            {
                SubmitTree(*lChild, InRenderer, InOutOrder);
            }
        }
    }

    // =============================================================================
    // CTOR
    // =============================================================================

    UICanvas::UICanvas(const float InReferenceHeight)
        : m_ReferenceHeight(InReferenceHeight)
        , m_Root(MakeUnique<UIPanel>())
    {
        // The root IS the visible rect: stretched over it, and never a hit — the canvas covers the
        // whole screen, so "the root was hit" would mean "the pointer is on screen" (L29).
        m_Root->Name           = "Root";
        m_Root->Rect.AnchorMin = { 0.f, 0.f };
        m_Root->Rect.AnchorMax = { 1.f, 1.f };
        m_Root->Rect.SizeDelta = { 0.f, 0.f };
        m_Root->bHitTestable   = false;
        m_Root->SetCanvasRecursive(this);
    }

    // =============================================================================
    // Space
    // =============================================================================

    void UICanvas::SetReferenceHeight(const float InHeight)
    {
        m_ReferenceHeight = InHeight;
        SetTargetSize(m_TargetWidth, m_TargetHeight);
    }

    void UICanvas::SetTargetSize(const Uint32 InWidth, const Uint32 InHeight)
    {
        m_TargetWidth  = InWidth;
        m_TargetHeight = InHeight;

        // Width follows the aspect (CAM2). Equal aspects divide to the same float, so a same-aspect
        // resize lands on the identical rect and dirties nothing — no epsilon, by construction.
        const float    lAspect = InHeight > 0 ? static_cast<float>(InWidth) / static_cast<float>(InHeight) : 0.f;
        const Bounds2D lNext   = Bounds2D::FromCenterSize({ 0.f, 0.f }, { m_ReferenceHeight * lAspect, m_ReferenceHeight });

        if (lNext.HalfExtent != m_VisibleBounds.HalfExtent)
        {
            m_VisibleBounds   = lNext;
            m_bVisibleChanged = true;
        }
    }

    CameraView UICanvas::MakeView() const noexcept
    {
        return CameraView{ { 0.f, 0.f }, m_ReferenceHeight * 0.5f };
    }

    Vector2F UICanvas::ScreenToCanvas(const Vector2F& InPixel) const noexcept
    {
        return ScreenToWorld(MakeView(),
                             { static_cast<float>(m_TargetWidth), static_cast<float>(m_TargetHeight) },
                             InPixel);
    }

    // =============================================================================
    // Frame
    // =============================================================================

    UICanvasStats UICanvas::Update(const UIBuildContext& InContext)
    {
        UICanvasStats lStats;

        m_Root->UpdateTree(m_VisibleBounds, m_bVisibleChanged, InContext, lStats);
        m_bVisibleChanged = false;

        return lStats;
    }

    void UICanvas::Submit(Renderer2D& InRenderer) const
    {
        Int32 lOrder = 0;
        SubmitTree(*m_Root, InRenderer, lOrder);

        if (lOrder >= MAX_ORDER && !m_bWarnedOrderOverflow)
        {
            OPAAX_LOG(LogUI, Warn, "UICanvas::Submit — more than {} quads; draw order past that is undefined.", MAX_ORDER);
            m_bWarnedOrderOverflow = true;
        }
    }

    UIWidget* UICanvas::HitTest(const Vector2F& InCanvasPoint)
    {
        return m_Root->HitTest(InCanvasPoint);
    }

    // =============================================================================
    // Input
    // =============================================================================

    UIWidget* UICanvas::BubblePointer(UIWidget* InFrom, const UIPointerEvent& InEvent)
    {
        for (UIWidget* lNode = InFrom; lNode != nullptr; lNode = lNode->GetParent())
        {
            if (lNode->OnPointerEvent(InEvent) == EUIReply::Handled)
            {
                return lNode;
            }
        }
        return nullptr;
    }

    EUIReply UICanvas::RoutePointer(const UIPointerEvent& InEvent)
    {
        if (InEvent.Type == EUIPointerEventType::Move)
        {
            UIWidget* lHit = HitTest(InEvent.Position);
            if (lHit != m_Hovered)
            {
                // Enter/Leave are DELIVERED, not bubbled — Slate's shape.
                if (m_Hovered != nullptr)
                {
                    m_Hovered->OnPointerEvent({ EUIPointerEventType::Leave, InEvent.Position, InEvent.Button });
                }
                m_Hovered = lHit;
                if (m_Hovered != nullptr)
                {
                    m_Hovered->OnPointerEvent({ EUIPointerEventType::Enter, InEvent.Position, InEvent.Button });
                }
            }

            // A captured widget still tracks the move; otherwise the move itself is never "handled".
            if (m_Pressed != nullptr)
            {
                m_Pressed->OnPointerEvent(InEvent);
                return EUIReply::Handled;
            }
            return EUIReply::Unhandled;
        }

        if (InEvent.Type == EUIPointerEventType::Down)
        {
            UIWidget* lHandler = BubblePointer(HitTest(InEvent.Position), InEvent);
            if (lHandler != nullptr)
            {
                m_Pressed     = lHandler;
                m_PressButton = InEvent.Button;
                return EUIReply::Handled;
            }
            return EUIReply::Unhandled;
        }

        if (InEvent.Type == EUIPointerEventType::Up)
        {
            // The capturing widget hears its Up even released outside; else whatever is under it.
            UIWidget* lTarget  = (m_Pressed != nullptr) ? m_Pressed : HitTest(InEvent.Position);
            UIWidget* lHandler = BubblePointer(lTarget, InEvent);

            if (m_PressButton == InEvent.Button)
            {
                m_Pressed     = nullptr;
                m_PressButton = EUIPointerButton::None;
            }
            return lHandler != nullptr ? EUIReply::Handled : EUIReply::Unhandled;
        }

        return EUIReply::Unhandled;
    }

    EUIReply UICanvas::RouteKey(const UIKeyEvent& InEvent)
    {
        for (UIWidget* lNode = m_Focused; lNode != nullptr; lNode = lNode->GetParent())
        {
            if (lNode->OnKeyEvent(InEvent) == EUIReply::Handled)
            {
                return EUIReply::Handled;
            }
        }
        return EUIReply::Unhandled;
    }

    void UICanvas::SetFocus(UIWidget* InWidget) noexcept
    {
        m_Focused = InWidget;
    }

    void UICanvas::ClearPointer()
    {
        if (m_Hovered != nullptr)
        {
            m_Hovered->OnPointerEvent({ EUIPointerEventType::Leave, { 0.f, 0.f }, EUIPointerButton::None });
            m_Hovered = nullptr;
        }
        m_Pressed     = nullptr;
        m_PressButton = EUIPointerButton::None;
    }

    void UICanvas::OnDetached(UIWidget& InSubtreeRoot)
    {
        // Any pointer resting on a node under InSubtreeRoot must be forgotten before it dies.
        const auto lUnderSubtree = [&InSubtreeRoot](const UIWidget* InNode)
        {
            for (const UIWidget* lNode = InNode; lNode != nullptr; lNode = lNode->GetParent())
            {
                if (lNode == &InSubtreeRoot) { return true; }
            }
            return false;
        };

        if (lUnderSubtree(m_Hovered)) { m_Hovered = nullptr; }
        if (lUnderSubtree(m_Focused)) { m_Focused = nullptr; }
        if (lUnderSubtree(m_Pressed))
        {
            m_Pressed     = nullptr;
            m_PressButton = EUIPointerButton::None;
        }
    }
}
