#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "UI/UIEvents.h"
#include "UI/UIAssetProvider.h"
#include "UI/UIQuad.h"
#include "UI/UIRect.h"

namespace Opaax
{
    class  ObjectClassRegistry;
    class  UICanvas;
    struct UICanvasStats;
    class  UIBindingTable;

    // =============================================================================
    // UIWidget — one node of a canvas tree, an Object: its class is the name a .opaaxui file saves,
    //   its properties are the fields it saves (a new widget type is a subclass with OPAAX_OBJECT,
    //   OPAAX_PROPERTIES and one OPAAX_REGISTER_OBJECT line). A parent owns its children. Does not
    //   know about the World.
    //
    //   Invalidation, not polling: code goes through the setters, which mark what is dirty;
    //   the editor writes fields directly then calls InvalidateLayout(). An idle canvas does no work.
    //   Flags: Layout (rect), Content (quads), Subtree (something below is dirty).
    //   Visibility, opacity and hit-testability are read when drawing / hit-testing.
    //   A Rebuild whose input is not ready (atlas uploading) calls InvalidateContent() and runs
    //   again next frame.
    //
    //   Input bubbles (UIEvents.h). The base lets events through.
    // =============================================================================
    class UIWidget : public Object
    {
        OPAAX_ABSTRACT_OBJECT(UIWidget, Object)

        friend class UICanvas;

        // =============================================================================
        // Authored state (public for reflection; use the setters in code)
        // =============================================================================
    public:
        OpaaxString Name;
        UIRect      Rect;
        bool        bVisible     = true;
        /** False lets clicks pass through (Unity's raycastTarget). */
        bool        bHitTestable = true;
        /**
         * Multiplied down the tree (fading a panel fades its children). Drawing only: a widget at 0
         * is still hit (use bVisible to stop that).
         */
        float       Opacity      = 1.f;

        OPAAX_PROPERTIES(UIWidget,
                         OPAAX_PROP(Name),
                         OPAAX_PROP(Rect),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(bHitTestable),
                         OPAAX_PROP(Opacity).SetRange(0.f, 1.f).SetDragStep(0.01f))

        void SetRect(const UIRect& InRect);

        // =============================================================================
        // Making one by class
        // =============================================================================
    public:
        /**
         * A new widget of the class named InClassName (what a file or the editor's Add menu names).
         * @return Empty when InClasses has no such class, or it is abstract or not a widget
         */
        static TStrongObjectPtr<UIWidget> Create(OpaaxStringID InClassName, const ObjectClassRegistry& InClasses);

        // =============================================================================
        // Tree
        // =============================================================================
    public:
        UIWidget*                                     GetParent()   const noexcept { return m_Parent; }
        const TDynArray<TStrongObjectPtr<UIWidget>>&  GetChildren() const noexcept { return m_Children; }

        /** Takes the child (its only owner should be the one given); drawn after (on top of) its siblings. */
        UIWidget* AddChild(TStrongObjectPtr<UIWidget> InChild);

        /** Takes the child, at InIndex among my children (clamped). */
        UIWidget* AddChild(TStrongObjectPtr<UIWidget> InChild, Uint64 InIndex);

        /** Gives the child back (for undo). Empty if InChild is not mine. */
        TStrongObjectPtr<UIWidget> RemoveChild(UIWidget& InChild);

        /** The canvas this node is in, or null when detached. */
        UICanvas* GetCanvas() const noexcept { return m_Canvas; }

        /**
         * Whether I lay out my children (a container). Then a child's anchors and position are ignored:
         * its Rect.SizeDelta is its desired size and my slot is its rect.
         */
        bool ArrangesChildren() const noexcept { return m_bArrangesChildren; }

        /**
         * The first widget named InName (me or a descendant, depth-first), or null.
         * Gameplay uses it to find authored widgets after loading a .opaaxui.
         */
        UIWidget* FindByName(const OpaaxString& InName);

        // =============================================================================
        // Input — override to handle an event; the default lets it bubble
        // =============================================================================
    public:
        virtual EUIReply OnPointerEvent(const UIPointerEvent& InEvent) { (void)InEvent; return EUIReply::Unhandled; }
        virtual EUIReply OnKeyEvent(const UIKeyEvent& InEvent)         { (void)InEvent; return EUIReply::Unhandled; }

        // =============================================================================
        // Bindings — read once per frame, before the walk
        // =============================================================================
    public:
        /** Reads my bound fields and invalidates only if a value changed. */
        virtual void OnPullBindings(UIBindingTable& InBindings) { (void)InBindings; }

        // =============================================================================
        // Invalidation
        // =============================================================================
    public:
        /** My rect must be resolved again (and my quads, and every descendant's rect). */
        void InvalidateLayout();

        /** My quads must be rebuilt (the rect is unchanged). */
        void InvalidateContent();

        // =============================================================================
        // Resolved state (from the last canvas Update)
        // =============================================================================
    public:
        const Bounds2D&          GetBounds() const noexcept { return m_Bounds; }
        const TDynArray<UIQuad>& GetQuads()  const noexcept { return m_Quads; }

        // =============================================================================
        // Leaf contract
        // =============================================================================
    protected:
        /**
         * Emits my quads for the resolved bounds. Called only when content is dirty.
         * Call InvalidateContent() from here to run again next frame.
         */
        virtual void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
        {
            (void)InContext; (void)OutQuads;
        }

        /**
         * My rect inside my parent. Default: ResolveRect. Overridden by UISafeArea (insets).
         */
        virtual Bounds2D ResolveBounds(const Bounds2D& InParentBounds) const
        {
            return ResolveRect(Rect, InParentBounds);
        }

        /**
         * Containers: one slot per child, in child order, inside my bounds. Called only when
         * m_bArrangesChildren is set. A child's rect is its slot.
         */
        virtual void ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const { (void)OutSlots; }

        bool m_bArrangesChildren = false;

        // =============================================================================
        // Internal — the canvas's walk
        // =============================================================================
    private:
        /**
         * Resolves and rebuilds what is dirty, descends where marked. bInParentChanged forces a resolve.
         * InSlot, when given, is my rect (my parent placed me).
         */
        void UpdateTree(const Bounds2D& InParentBounds, const Bounds2D* InSlot, bool bInParentChanged,
                        const UIBuildContext& InContext, UICanvasStats& OutStats);

        /** The deepest visible, hit-testable widget (or me) containing InPoint, top-most first. */
        UIWidget* HitTest(const Vector2F& InPoint);

        /** OnPullBindings on me, then every descendant. */
        void PullBindings(UIBindingTable& InBindings);

        void MarkSubtreeUp();
        void SetCanvasRecursive(UICanvas* InCanvas);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UIWidget*                             m_Parent = nullptr;   // not owned
        UICanvas*                             m_Canvas = nullptr;   // not owned; set on attach
        TDynArray<TStrongObjectPtr<UIWidget>> m_Children;

        Bounds2D           m_Bounds;
        TDynArray<UIQuad>  m_Quads;

        bool m_bLayoutDirty  = true;
        bool m_bContentDirty = true;
        bool m_bSubtreeDirty = false;
    };
}
