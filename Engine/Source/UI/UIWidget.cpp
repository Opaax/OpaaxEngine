#include "UI/UIWidget.h"

#include <algorithm>

#include "UI/UICanvas.h"

namespace Opaax
{
    // =============================================================================
    // Authored state
    // =============================================================================

    void UIWidget::SetRect(const UIRect& InRect)
    {
        Rect = InRect;
        InvalidateLayout();
    }

    // =============================================================================
    // Tree
    // =============================================================================

    UIWidget* UIWidget::AddChild(TUniquePtr<UIWidget> InChild)
    {
        if (!InChild)
        {
            return nullptr;
        }

        UIWidget* lChild = InChild.get();
        lChild->m_Parent = this;
        m_Children.emplace_back(std::move(InChild));

        // A new child arrives with fresh flags; the walk only needs to reach it.
        m_bSubtreeDirty = true;
        MarkSubtreeUp();

        return lChild;
    }

    TUniquePtr<UIWidget> UIWidget::RemoveChild(UIWidget& InChild)
    {
        const auto lIt = std::find_if(m_Children.begin(), m_Children.end(),
                                      [&InChild](const TUniquePtr<UIWidget>& InOwned) { return InOwned.get() == &InChild; });
        if (lIt == m_Children.end())
        {
            return nullptr;
        }

        TUniquePtr<UIWidget> lRemoved = std::move(*lIt);
        m_Children.erase(lIt);

        lRemoved->m_Parent        = nullptr;
        lRemoved->m_bLayoutDirty  = true;   // it will be resolved against a new parent, if any
        lRemoved->m_bContentDirty = true;

        return lRemoved;
    }

    // =============================================================================
    // Invalidation
    // =============================================================================

    void UIWidget::InvalidateLayout()
    {
        m_bLayoutDirty  = true;
        m_bContentDirty = true;
        MarkSubtreeUp();
    }

    void UIWidget::InvalidateContent()
    {
        m_bContentDirty = true;
        MarkSubtreeUp();
    }

    void UIWidget::MarkSubtreeUp()
    {
        // A set flag implies every ancestor's is set (only this walk sets them; the update clears
        // top-down after descending), so the first one already marked ends the climb.
        for (UIWidget* lNode = m_Parent; lNode != nullptr && !lNode->m_bSubtreeDirty; lNode = lNode->m_Parent)
        {
            lNode->m_bSubtreeDirty = true;
        }
    }

    // =============================================================================
    // Internal — the canvas's walk
    // =============================================================================

    void UIWidget::UpdateTree(const Bounds2D& InParentBounds, bool bInParentChanged, UICanvasStats& OutStats)
    {
        bool lChanged = false;

        if (m_bLayoutDirty || bInParentChanged)
        {
            const Bounds2D lBounds = ResolveRect(Rect, InParentBounds);
            ++OutStats.Layouts;

            // Same rect from a changed parent (a corner-anchored child of a widening root): nothing
            // below me moved, so nothing below me is re-resolved.
            lChanged = lBounds.Center != m_Bounds.Center || lBounds.HalfExtent != m_Bounds.HalfExtent;
            m_Bounds = lBounds;
            if (lChanged)
            {
                m_bContentDirty = true;
            }
        }

        if (m_bContentDirty)
        {
            m_Quads.clear();
            Rebuild(m_Quads);
            ++OutStats.Rebuilds;
        }

        if (lChanged || m_bSubtreeDirty)
        {
            for (const TUniquePtr<UIWidget>& lChild : m_Children)
            {
                lChild->UpdateTree(m_Bounds, lChanged, OutStats);
            }
        }

        m_bLayoutDirty  = false;
        m_bContentDirty = false;
        m_bSubtreeDirty = false;
    }

    const UIWidget* UIWidget::HitTest(const Vector2F& InPoint) const
    {
        if (!bVisible)
        {
            return nullptr;
        }

        // Drawn last = on top, so the last child is asked first.
        for (auto lIt = m_Children.rbegin(); lIt != m_Children.rend(); ++lIt)
        {
            if (const UIWidget* lHit = (*lIt)->HitTest(InPoint))
            {
                return lHit;
            }
        }

        return (bHitTestable && m_Bounds.Contains(InPoint)) ? this : nullptr;
    }
}
