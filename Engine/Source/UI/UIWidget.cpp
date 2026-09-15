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
        return AddChild(std::move(InChild), m_Children.size());
    }

    UIWidget* UIWidget::AddChild(TUniquePtr<UIWidget> InChild, const Uint64 InIndex)
    {
        if (!InChild)
        {
            return nullptr;
        }

        UIWidget* lChild = InChild.get();
        lChild->m_Parent = this;
        lChild->SetCanvasRecursive(m_Canvas);
        m_Children.emplace(m_Children.begin() + static_cast<std::ptrdiff_t>(std::min(InIndex, static_cast<Uint64>(m_Children.size()))),
                           std::move(InChild));

        // A new child arrives with fresh flags; the walk only needs to reach it — unless I place my
        // children, in which case every sibling's slot may have moved.
        m_bSubtreeDirty = true;
        MarkSubtreeUp();
        if (m_bArrangesChildren) { InvalidateLayout(); }

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

        if (m_bArrangesChildren) { InvalidateLayout(); }   // the siblings after it close the gap

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
        InOutJson["Opacity"]      = Opacity;
    }

    void UIWidget::LoadFields(const nlohmann::json& InJson)
    {
        Name         = InJson.value("Name", Name);
        Rect         = InJson.value("Rect", Rect);
        bVisible     = InJson.value("bVisible", bVisible);
        bHitTestable = InJson.value("bHitTestable", bHitTestable);
        Opacity      = InJson.value("Opacity", Opacity);
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

        // UP, through every ancestor whose layout depends on its children, and no further: my size
        // shifts my siblings under a container, and the container's size may shift ITS siblings.
        // A plain panel's rect owes nothing to what is under it, so the climb stops there.
        if (m_Parent != nullptr && m_Parent->m_bArrangesChildren && !m_Parent->m_bLayoutDirty)
        {
            m_Parent->InvalidateLayout();
        }
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

    void UIWidget::UpdateTree(const Bounds2D& InParentBounds, const Bounds2D* InSlot, const bool bInParentChanged,
                              const UIBuildContext& InContext, UICanvasStats& OutStats)
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
            // Arranged by my parent: the slot IS my rect. Otherwise my anchors say where I sit.
            const Bounds2D lBounds = InSlot != nullptr ? *InSlot : ResolveBounds(InParentBounds);
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

        if (!m_bArrangesChildren)
        {
            if (!lChanged && !lSubtree)
            {
                return;
            }

            for (const TUniquePtr<UIWidget>& lChild : m_Children)
            {
                lChild->UpdateTree(m_Bounds, nullptr, lChanged, InContext, OutStats);
            }
            return;
        }

        // A container hands each child its slot, and the slots depend on MY fields (spacing,
        // alignment, a child added) as much as on my rect — so a re-layout of me re-arranges even
        // when my rect came out the same. Each child re-resolves when I was re-laid; the same-rect
        // compare above then stops what did not move. Cheap, and only reached when I or something
        // under me is dirty.
        if (!lLayout && !lSubtree)
        {
            return;
        }

        TDynArray<Bounds2D> lSlots;
        ArrangeChildren(lSlots);

        for (Uint64 lIndex = 0; lIndex < m_Children.size(); ++lIndex)
        {
            const Bounds2D* lSlot = lIndex < lSlots.size() ? &lSlots[lIndex] : nullptr;
            m_Children[lIndex]->UpdateTree(m_Bounds, lSlot, lLayout, InContext, OutStats);
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
