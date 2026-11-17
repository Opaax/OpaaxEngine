#include "UI/UIWidget.h"

#include "Core/Algo/Algo.h"

#include "Core/Maths/Maths.h"
#include "Object/ObjectClassRegistry.h"
#include "UI/UICanvas.h"

namespace Opaax
{
    UIWidget::~UIWidget()
    {
        m_bDestroying = true;
        for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
        {
            lModifier->OnDetach();
            lModifier->m_Owner = nullptr;
        }
    }

    // =============================================================================
    // Authored state
    // =============================================================================

    void UIWidget::SetRect(const UIRect& InRect)
    {
        Rect = InRect;
        InvalidateLayout();
    }

    // =============================================================================
    // Making one by class
    // =============================================================================

    TStrongObjectPtr<UIWidget> UIWidget::Create(const OpaaxStringID InClassName, const ObjectClassRegistry& InClasses)
    {
        const ObjectClass* lClass = InClasses.FindByName(InClassName);

        if (lClass == nullptr || lClass->IsAbstract() || !lClass->IsChildOf(StaticClass()))
        {
            return {};
        }

        return Cast<UIWidget>(lClass->CreateInstance());
    }

    // =============================================================================
    // Tree
    // =============================================================================

    UIWidget* UIWidget::AddChild(TStrongObjectPtr<UIWidget> InChild)
    {
        return AddChild(Move(InChild), m_Children.size());
    }

    UIWidget* UIWidget::AddChild(TStrongObjectPtr<UIWidget> InChild, const Uint64 InIndex)
    {
        if (!InChild)
        {
            return nullptr;
        }

        UIWidget* lChild = InChild.Get();
        lChild->m_Parent = this;
        lChild->SetCanvasRecursive(m_Canvas);
        m_Children.emplace(m_Children.begin() + static_cast<PtrDiff>(Maths::Min(InIndex, static_cast<Uint64>(m_Children.size()))),
                           Move(InChild));

        // The new child only needs to be reached by the walk, unless I lay out my children
        // (then every slot may have moved).
        m_bSubtreeDirty = true;
        MarkSubtreeUp();
        if (m_bArrangesChildren) { InvalidateLayout(); }

        return lChild;
    }

    TStrongObjectPtr<UIWidget> UIWidget::RemoveChild(UIWidget& InChild)
    {
        const auto lIt = Algo::FindIf(m_Children,
                                      [&InChild](const TStrongObjectPtr<UIWidget>& InOwned) { return InOwned.Get() == &InChild; });
        if (lIt == m_Children.end())
        {
            return nullptr;
        }

        TStrongObjectPtr<UIWidget> lRemoved = Move(*lIt);
        m_Children.erase(lIt);

        // Clear pointer state under this subtree before it can be destroyed.
        if (m_Canvas != nullptr)
        {
            lRemoved->m_Canvas->OnDetached(*lRemoved);
        }

        lRemoved->m_Parent        = nullptr;
        lRemoved->SetCanvasRecursive(nullptr);
        lRemoved->m_bLayoutDirty  = true;   // resolved again against its new parent
        lRemoved->m_bContentDirty = true;

        if (m_bArrangesChildren) { InvalidateLayout(); }   // the following siblings move

        return lRemoved;
    }

    UIWidget* UIWidget::FindByName(const OpaaxString& InName)
    {
        if (Name == InName)
        {
            return this;
        }

        for (const TStrongObjectPtr<UIWidget>& lChild : m_Children)
        {
            if (UIWidget* lFound = lChild->FindByName(InName))
            {
                return lFound;
            }
        }

        return nullptr;
    }

    // =============================================================================
    // Modifiers
    // =============================================================================

    UIModifier* UIWidget::AddModifier(TStrongObjectPtr<UIModifier> InModifier)
    {
        return AddModifier(Move(InModifier), m_Modifiers.size());
    }

    UIModifier* UIWidget::AddModifier(TStrongObjectPtr<UIModifier> InModifier, const Uint64 InIndex)
    {
        if (!InModifier || InModifier->m_Owner != nullptr)
        {
            return nullptr;
        }

        UIModifier* lModifier = InModifier.Get();
        lModifier->m_Owner    = this;
        m_Modifiers.emplace(m_Modifiers.begin() + static_cast<PtrDiff>(Maths::Min(InIndex, static_cast<Uint64>(m_Modifiers.size()))),
                            Move(InModifier));

        lModifier->OnAttach();

        // Woken before it had a canvas to tick it: this one does.
        if (lModifier->m_bAwake && m_Canvas != nullptr)
        {
            m_Canvas->AddAwake(*lModifier);
        }
        return lModifier;
    }

    // =============================================================================
    // Interaction state
    // =============================================================================

    namespace
    {
        /** Whether InNode is InWidget or under it. */
        bool IsSelfOrUnder(const UIWidget& InWidget, const UIWidget* InNode)
        {
            for (; InNode != nullptr; InNode = InNode->GetParent())
            {
                if (InNode == &InWidget) { return true; }
            }
            return false;
        }
    }

    EUIInteractionState UIWidget::GetInteractionState() const
    {
        if (!IsInteractable())
        {
            return EUIInteractionState::Disabled;
        }
        if (m_Canvas == nullptr)
        {
            return EUIInteractionState::Normal;
        }

        // A press shows only while the pointer is over it: leaving cancels a button's press.
        const bool bHovered = IsSelfOrUnder(*this, m_Canvas->GetHovered());
        if (bHovered && IsSelfOrUnder(*this, m_Canvas->GetPressed()))
        {
            return EUIInteractionState::Pressed;
        }
        if (bHovered)
        {
            return EUIInteractionState::Hovered;
        }
        if (IsSelfOrUnder(*this, m_Canvas->GetFocus()))
        {
            return EUIInteractionState::Focused;
        }
        return EUIInteractionState::Normal;
    }

    // =============================================================================
    // Show and hide
    // =============================================================================

    void UIWidget::Show()
    {
        // Cancelled before the modifiers hear it, so a hide tween stopping now does not finish the hide.
        m_bHiding     = false;
        m_HideWaiting = 0;

        bVisible = true;   // drawing reads it: nothing to invalidate

        if (IsShownInTree())
        {
            ShowRecursive();
        }
    }

    void UIWidget::Hide()
    {
        if (!bVisible || m_bHiding)
        {
            return;
        }

        m_bHiding     = true;
        m_HideWaiting = 0;

        // A modifier that waits finishes later, from its Tick: never inside OnHide.
        for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
        {
            if (lModifier->bEnabled && lModifier->OnHide())
            {
                ++m_HideWaiting;
            }
        }

        if (m_HideWaiting == 0)
        {
            m_bHiding = false;
            bVisible  = false;
        }
    }

    void UIWidget::OnHideStepDone()
    {
        if (!m_bHiding || m_bDestroying)
        {
            return;
        }

        if (m_HideWaiting > 0)
        {
            --m_HideWaiting;
        }
        if (m_HideWaiting == 0)
        {
            m_bHiding = false;
            bVisible  = false;
        }
    }

    void UIWidget::ShowRecursive()
    {
        for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
        {
            lModifier->OnShow();
        }
        for (const TStrongObjectPtr<UIWidget>& lChild : m_Children)
        {
            if (lChild->bVisible)
            {
                lChild->ShowRecursive();
            }
        }
    }

    bool UIWidget::IsShownInTree() const noexcept
    {
        for (const UIWidget* lNode = this; lNode != nullptr; lNode = lNode->m_Parent)
        {
            if (!lNode->bVisible) { return false; }
        }
        return true;
    }

    void UIWidget::NotifyStateChanged()
    {
        const EUIInteractionState lNow = GetInteractionState();
        if (lNow == m_State)
        {
            return;
        }

        const EUIInteractionState lWas = m_State;
        m_State = lNow;
        for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
        {
            lModifier->OnStateChanged(lWas, lNow);
        }
    }

    TStrongObjectPtr<UIModifier> UIWidget::RemoveModifier(UIModifier& InModifier)
    {
        const auto lIt = Algo::FindIf(m_Modifiers,
                                      [&InModifier](const TStrongObjectPtr<UIModifier>& InOwned) { return InOwned.Get() == &InModifier; });
        if (lIt == m_Modifiers.end())
        {
            return nullptr;
        }

        TStrongObjectPtr<UIModifier> lRemoved = Move(*lIt);
        m_Modifiers.erase(lIt);

        lRemoved->OnDetach();
        lRemoved->m_Owner = nullptr;
        return lRemoved;
    }

    bool UIWidget::MoveModifier(UIModifier& InModifier, const Uint64 InIndex)
    {
        const auto lIt = Algo::FindIf(m_Modifiers,
                                      [&InModifier](const TStrongObjectPtr<UIModifier>& InOwned) { return InOwned.Get() == &InModifier; });
        if (lIt == m_Modifiers.end())
        {
            return false;
        }

        // Taken out and put back without a detach: it stays mine.
        TStrongObjectPtr<UIModifier> lMoved = Move(*lIt);
        m_Modifiers.erase(lIt);

        const Uint64 lIndex = Maths::Min(InIndex, static_cast<Uint64>(m_Modifiers.size()));
        m_Modifiers.emplace(m_Modifiers.begin() + static_cast<PtrDiff>(lIndex), Move(lMoved));
        return true;
    }

    void UIWidget::SetCanvasRecursive(UICanvas* InCanvas)
    {
        m_Canvas = InCanvas;

        // Modifiers woken while out of a canvas start ticking in this one; joining a ticked canvas
        // shown, I am shown.
        if (m_Canvas != nullptr)
        {
            const bool bShown = m_Canvas->IsTicked() && IsShownInTree();
            for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
            {
                if (lModifier->m_bAwake) { m_Canvas->AddAwake(*lModifier); }
                if (bShown)              { lModifier->OnShow(); }
            }
        }

        for (const TStrongObjectPtr<UIWidget>& lChild : m_Children)
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
        m_bPoseDirty    = m_bPoseDirty || !m_Modifiers.empty();   // the editor's way to say a modifier was edited
        MarkSubtreeUp();

        // Up through every ancestor whose layout depends on its children (containers).
        // A plain panel's rect does not, so the climb stops there.
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

    void UIWidget::InvalidatePose()
    {
        if (m_bDestroying)
        {
            return;
        }

        m_bPoseDirty = true;
        MarkSubtreeUp();
    }

    // =============================================================================
    // Poses
    // =============================================================================

    void UIWidget::ComposePose()
    {
        m_Pose = UIComposedPose{};

        for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
        {
            const UIPoseModifier* lPoser = Cast<UIPoseModifier>(lModifier.Get());

            UIPose lPose;
            if (lPoser == nullptr || !lPoser->bEnabled || !lPoser->GetPose(lPose))
            {
                continue;
            }

            const Vector2F lMin   = m_Bounds.Min();
            const Vector2F lSize  = m_Bounds.Size();
            const Vector2F lPivot = { lMin.x + lPoser->Pivot.x * lSize.x, lMin.y + lPoser->Pivot.y * lSize.y };
            const Affine2D lStep  = Affine2D::FromPose(lPose.Translation, lPose.Rotation, lPose.Scale, lPivot);

            // On top of the earlier ones: applied after them.
            m_Pose.Draw = lStep * m_Pose.Draw;
            if (lPoser->bMovesHitArea)
            {
                m_Pose.Hit = lStep * m_Pose.Hit;
            }

            m_Pose.Opacity *= lPose.Opacity;
            m_Pose.Tint     = { m_Pose.Tint.x * lPose.Tint.r, m_Pose.Tint.y * lPose.Tint.g,
                                m_Pose.Tint.z * lPose.Tint.b, m_Pose.Tint.w * lPose.Tint.a };
            m_Pose.bRest    = false;
        }
    }

    Affine2D UIWidget::GetDrawTransform() const noexcept
    {
        const Affine2D lParent = m_Parent != nullptr ? m_Parent->GetDrawTransform() : Affine2D::Identity();
        return m_Pose.bRest ? lParent : lParent * m_Pose.Draw;
    }

    Affine2D UIWidget::GetHitTransform() const noexcept
    {
        const Affine2D lParent = m_Parent != nullptr ? m_Parent->GetHitTransform() : Affine2D::Identity();
        return m_Pose.bRest ? lParent : lParent * m_Pose.Hit;
    }

    Vector2F UIWidget::CanvasToLayout(const Vector2F& InCanvasPoint) const noexcept
    {
        const Affine2D lHit = GetHitTransform();
        if (lHit.IsIdentity())
        {
            return InCanvasPoint;
        }

        Affine2D lInverse;
        if (!lHit.Inverse(lInverse))
        {
            const float lFar = TNumericLimits<float>::max();
            return { lFar, lFar };
        }
        return lInverse.TransformPoint(InCanvasPoint);
    }

    float UIWidget::GetDrawnOpacity() const noexcept
    {
        const float lParent = m_Parent != nullptr ? m_Parent->GetDrawnOpacity() : 1.f;
        return lParent * Opacity * m_Pose.Opacity;
    }

    void UIWidget::MarkSubtreeUp()
    {
        // An already marked ancestor means all above it are marked too: stop.
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
        // Snapshot, then clear: anything set from now on is next frame's work.
        const bool lLayout  = m_bLayoutDirty || bInParentChanged;
        bool       lContent = m_bContentDirty;
        const bool lSubtree = m_bSubtreeDirty;
        const bool lPose    = m_bPoseDirty;

        m_bLayoutDirty  = false;
        m_bContentDirty = false;
        m_bSubtreeDirty = false;
        m_bPoseDirty    = false;

        bool lChanged = false;

        if (lLayout)
        {
            // Placed by my parent: the slot is my rect. Otherwise my anchors decide.
            const Bounds2D lBounds = InSlot != nullptr ? *InSlot : ResolveBounds(InParentBounds);
            ++OutStats.Layouts;

            // Same rect from a changed parent: nothing below me moved.
            lChanged = lBounds.Center != m_Bounds.Center || lBounds.HalfExtent != m_Bounds.HalfExtent;
            m_Bounds = lBounds;
            lContent = lContent || lChanged;
        }

        // Pivots are points of the layout rect: a moved rect moves them.
        if (lPose || (lChanged && !m_Modifiers.empty()))
        {
            ComposePose();
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

            for (const TStrongObjectPtr<UIWidget>& lChild : m_Children)
            {
                lChild->UpdateTree(m_Bounds, nullptr, lChanged, InContext, OutStats);
            }
            return;
        }

        // A container's slots depend on its fields too, so a re-layout always re-arranges the
        // children; the same-rect check above stops what did not move.
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

    void UIWidget::PullBindings(UIBindingTable& InBindings)
    {
        OnPullBindings(InBindings);

        for (const TStrongObjectPtr<UIWidget>& lChild : m_Children)
        {
            lChild->PullBindings(InBindings);
        }
    }

    UIWidget* UIWidget::HitTest(const Vector2F& InPoint, const Affine2D& InParentHit, const bool bInParentPosed)
    {
        // Hiding: still drawn while it fades, not there for the pointer.
        if (!bVisible || m_bHiding)
        {
            return nullptr;
        }

        // My hit area: my parents' moving poses, then mine. Nothing posed: the point as it is.
        Affine2D lHit    = InParentHit;
        bool     bPosed  = bInParentPosed;
        if (!m_Pose.bRest && !m_Pose.Hit.IsIdentity())
        {
            lHit   = InParentHit * m_Pose.Hit;
            bPosed = true;
        }

        Vector2F lLocal = InPoint;
        if (bPosed)
        {
            Affine2D lInverse;
            if (!lHit.Inverse(lInverse))
            {
                return nullptr;   // flattened (a scale of 0): nothing of it, or under it, can be hit
            }
            lLocal = lInverse.TransformPoint(InPoint);
        }

        // Drawn last = on top, so the last child is checked first. What I clip is not there outside my
        // rect, for the pointer either.
        if (!m_bClipsChildren || m_Bounds.Contains(lLocal))
        {
            for (auto lIt = m_Children.rbegin(); lIt != m_Children.rend(); ++lIt)
            {
                if (UIWidget* lFound = (*lIt)->HitTest(InPoint, lHit, bPosed))
                {
                    return lFound;
                }
            }
        }

        return (bHitTestable && m_Bounds.Contains(lLocal)) ? this : nullptr;
    }
}
