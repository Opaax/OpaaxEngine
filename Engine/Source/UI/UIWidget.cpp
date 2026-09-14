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
        lChild->SetCanvasRecursive(m_Canvas);
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

        // Drop any pointer state resting under this subtree BEFORE the pointers can die — no walk
        // ever touches freed memory looking for a stale hovered/pressed/focused widget.
        if (m_Canvas != nullptr)
        {
            lRemoved->m_Canvas->OnDetached(*lRemoved);
        }

        lRemoved->m_Parent        = nullptr;
        lRemoved->SetCanvasRecursive(nullptr);
        lRemoved->m_bLayoutDirty  = true;   // it will be resolved against a new parent, if any
        lRemoved->m_bContentDirty = true;

        return lRemoved;
    }

    UIWidget* UIWidget::FindByName(const OpaaxString& InName)
    {
        if (Name == InName)
        {
            return this;
        }

        for (const TUniquePtr<UIWidget>& lChild : m_Children)
        {
            if (UIWidget* lFound = lChild->FindByName(InName))
            {
                return lFound;
            }
        }

        return nullptr;
    }

    // =============================================================================
    // Serialization
    // =============================================================================

    void UIWidget::SaveFields(nlohmann::json& InOutJson) const
    {
        InOutJson["Name"]         = Name;
        InOutJson["Rect"]         = Rect;
        InOutJson["bVisible"]     = bVisible;
        InOutJson["bHitTestable"] = bHitTestable;
    }

    void UIWidget::LoadFields(const nlohmann::json& InJson)
    {
        Name         = InJson.value("Name", Name);
        Rect         = InJson.value("Rect", Rect);
        bVisible     = InJson.value("bVisible", bVisible);
        bHitTestable = InJson.value("bHitTestable", bHitTestable);
    }

    void UIWidget::SetCanvasRecursive(UICanvas* InCanvas)
    {
        m_Canvas = InCanvas;
        for (const TUniquePtr<UIWidget>& lChild : m_Children)
        {
            lChild->SetCanvasRecursive(InCanvas);
        }
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

    void UIWidget::UpdateTree(const Bounds2D& InParentBounds, bool bInParentChanged, const UIBuildContext& InContext,
                              UICanvasStats& OutStats)
    {
        // Snapshot, then clear: anything set from here on (a Rebuild re-arming, a child marking
        // me) is next frame's work and survives.
        const bool lLayout  = m_bLayoutDirty || bInParentChanged;
        bool       lContent = m_bContentDirty;
        const bool lSubtree = m_bSubtreeDirty;

        m_bLayoutDirty  = false;
        m_bContentDirty = false;
        m_bSubtreeDirty = false;

        bool lChanged = false;

        if (lLayout)
        {
            const Bounds2D lBounds = ResolveRect(Rect, InParentBounds);
            ++OutStats.Layouts;

            // Same rect from a changed parent (a corner-anchored child of a widening root): nothing
            // below me moved, so nothing below me is re-resolved.
            lChanged = lBounds.Center != m_Bounds.Center || lBounds.HalfExtent != m_Bounds.HalfExtent;
            m_Bounds = lBounds;
            lContent = lContent || lChanged;
        }

        if (lContent)
        {
            m_Quads.clear();
            Rebuild(InContext, m_Quads);
            ++OutStats.Rebuilds;
        }

        if (lChanged || lSubtree)
        {
            for (const TUniquePtr<UIWidget>& lChild : m_Children)
            {
                lChild->UpdateTree(m_Bounds, lChanged, InContext, OutStats);
            }
        }
    }

    UIWidget* UIWidget::HitTest(const Vector2F& InPoint)
    {
        if (!bVisible)
        {
            return nullptr;
        }

        // Drawn last = on top, so the last child is asked first.
        for (auto lIt = m_Children.rbegin(); lIt != m_Children.rend(); ++lIt)
        {
            if (UIWidget* lHit = (*lIt)->HitTest(InPoint))
            {
                return lHit;
            }
        }

        return (bHitTestable && m_Bounds.Contains(InPoint)) ? this : nullptr;
    }
}
