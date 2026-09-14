#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIQuad.h"
#include "UI/UIRect.h"

namespace Opaax
{
    struct UICanvasStats;

    // =============================================================================
    // UIWidget — one node of a canvas tree. Knows nothing about the World: a canvas is an object
    //   anyone can own (the GameInstance, a world subsystem, an editor panel).
    //
    //   INVALIDATION, NOT POLLING. Authored fields are public so the editor's DrawProperties can
    //   write them, and DIRTYING IS A VERB: code goes through the setters, which know which flag
    //   they owe; the editor writes the field and calls InvalidateLayout(). Nothing compares
    //   state frame to frame — an idle canvas resolves no rect and rebuilds no quad.
    //
    //   Three flags: Layout (my rect must be re-resolved), Content (my quads must be rebuilt),
    //   Subtree (something below me is dirty — the walk descends only where this is set).
    //   Visibility and hit-testability are READ at submit / hit-test time and dirty nothing.
    // =============================================================================
    class OPAAX_API UIWidget
    {
        friend class UICanvas;

        // =============================================================================
        // CTOR / DTOR
        // =============================================================================
    public:
        UIWidget() = default;
        virtual ~UIWidget() = default;

        UIWidget(const UIWidget&)            = delete;
        UIWidget& operator=(const UIWidget&) = delete;
        UIWidget(UIWidget&&)                 = delete;
        UIWidget& operator=(UIWidget&&)      = delete;

        // =============================================================================
        // Authored state — public for reflection; the setters below are the route in code
        // =============================================================================
    public:
        OpaaxString Name;
        UIRect      Rect;
        bool        bVisible     = true;
        /** Unity's raycastTarget: false lets a click pass through to whatever is behind. */
        bool        bHitTestable = true;

        OPAAX_PROPERTIES(UIWidget,
                         OPAAX_PROP(Name),
                         OPAAX_PROP(Rect),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(bHitTestable))

        void SetRect(const UIRect& InRect);

        // =============================================================================
        // Identity
        // =============================================================================
    public:
        /** The registry key a file names and the noun a log line uses — "UIImage", never a label. */
        virtual OpaaxStringID GetTypeName() const noexcept = 0;

        // =============================================================================
        // Tree
        // =============================================================================
    public:
        UIWidget*                               GetParent()   const noexcept { return m_Parent; }
        const TDynArray<TUniquePtr<UIWidget>>&  GetChildren() const noexcept { return m_Children; }

        /** Take ownership; drawn after (on top of) its siblings. @return the child, for chaining. */
        UIWidget* AddChild(TUniquePtr<UIWidget> InChild);

        /** Hand ownership back — undo wants the node, not a copy. Null when InChild is not mine. */
        TUniquePtr<UIWidget> RemoveChild(UIWidget& InChild);

        // =============================================================================
        // Invalidation
        // =============================================================================
    public:
        /** My rect must be re-resolved — and so my quads, and every descendant's rect. */
        void InvalidateLayout();

        /** My quads must be rebuilt; the rect stands. */
        void InvalidateContent();

        // =============================================================================
        // Resolved state — what the last canvas Update produced
        // =============================================================================
    public:
        const Bounds2D&          GetBounds() const noexcept { return m_Bounds; }
        const TDynArray<UIQuad>& GetQuads()  const noexcept { return m_Quads; }

        // =============================================================================
        // Leaf contract
        // =============================================================================
    protected:
        /** Emit my quads for the resolved bounds. Called only when content is dirty. */
        virtual void Rebuild(TDynArray<UIQuad>& OutQuads) { (void)OutQuads; }

        // =============================================================================
        // Internal — the canvas's walk
        // =============================================================================
    private:
        /** Resolve + rebuild where dirty, descend where marked; bInParentChanged forces a resolve. */
        void UpdateTree(const Bounds2D& InParentBounds, bool bInParentChanged, UICanvasStats& OutStats);

        /** The deepest visible, hit-testable descendant (or me) containing InPoint; top-most first. */
        const UIWidget* HitTest(const Vector2F& InPoint) const;

        void MarkSubtreeUp();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UIWidget*                        m_Parent = nullptr;   // non-owning
        TDynArray<TUniquePtr<UIWidget>>  m_Children;

        Bounds2D           m_Bounds;
        TDynArray<UIQuad>  m_Quads;

        bool m_bLayoutDirty  = true;
        bool m_bContentDirty = true;
        bool m_bSubtreeDirty = false;
    };
}
