#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Templates/TypeTraits.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "UI/UIEvents.h"
#include "UI/UIAssetProvider.h"
#include "UI/UIModifier.h"
#include "UI/UIPose.h"
#include "UI/UIQuad.h"
#include "UI/UIRect.h"

namespace Opaax
{
    /** Whose interaction state a widget shows: its own, or that of the nearest button above it. */
    enum class EUIStateSource : Uint8
    {
        Self,
        Button   // an icon or a label inside a button reacts to the button's hover and press
    };

    inline const char* ToString(const EUIStateSource InSource) noexcept
    {
        return InSource == EUIStateSource::Button ? "Button" : "Self";
    }

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

    public:
        /** Detaches my modifiers before they go. */
        ~UIWidget() override;

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
        /**
         * Button: my state is that of the nearest button above me (an icon swaps, a label tints with
         * its button's hover and press), whatever widgets lie between; my own pointer state does not count.
         */
        EUIStateSource StateSource = EUIStateSource::Self;

        OPAAX_PROPERTIES(UIWidget,
                         OPAAX_PROP(Name),
                         OPAAX_PROP(Rect),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(bHitTestable),
                         OPAAX_PROP(Opacity).SetRange(0.f, 1.f).SetDragStep(0.01f),
                         OPAAX_PROP(StateSource).SetTooltip("Button: show the state of the nearest button above\n"
                                                            "(its hover, press, disabled), not my own."))

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
         * Whether my children show only inside my rect (a mask, a scroll box): drawn masked by me, and
         * hit only there.
         */
        bool ClipsChildren() const noexcept { return m_bClipsChildren; }

        /** The texture my children are masked with when I clip them; null clips them to my rect. */
        virtual ITexture2D* GetClipTexture() const noexcept { return nullptr; }

        /**
         * The first widget named InName (me or a descendant, depth-first), or null.
         * Gameplay uses it to find authored widgets after loading a .opaaxui.
         */
        UIWidget* FindByName(const OpaaxString& InName);

        // =============================================================================
        // Modifiers — owned, in the order added (UIModifier.h)
        // =============================================================================
    public:
        /** A new modifier of class T at the end of my list; its setters chain. */
        template<CDerivedFrom<UIModifier> T>
        T& AddModifier()
        {
            return static_cast<T&>(*AddModifier(NewObject<T>()));
        }

        /**
         * Takes the modifier (its only owner should be the one given), at the end of my list.
         * @return Null when InModifier is empty or already belongs to a widget
         */
        UIModifier* AddModifier(TStrongObjectPtr<UIModifier> InModifier);

        /** Takes the modifier, at InIndex in my list (clamped). */
        UIModifier* AddModifier(TStrongObjectPtr<UIModifier> InModifier, Uint64 InIndex);

        /** Gives the modifier back (for undo), detached. Empty if InModifier is not mine. */
        TStrongObjectPtr<UIModifier> RemoveModifier(UIModifier& InModifier);

        /** Moves InModifier to InIndex in my list (clamped). False if it is not mine. */
        bool MoveModifier(UIModifier& InModifier, Uint64 InIndex);

        /** My first modifier of class T (or a subclass), or null. */
        template<CDerivedFrom<UIModifier> T>
        T* FindModifier() const noexcept
        {
            for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
            {
                if (lModifier->IsA<T>()) { return static_cast<T*>(lModifier.Get()); }
            }
            return nullptr;
        }

        /** My first modifier of class T named InName, or null. */
        template<CDerivedFrom<UIModifier> T>
        T* FindModifier(const OpaaxStringID InName) const noexcept
        {
            for (const TStrongObjectPtr<UIModifier>& lModifier : m_Modifiers)
            {
                if (lModifier->Name == InName && lModifier->IsA<T>()) { return static_cast<T*>(lModifier.Get()); }
            }
            return nullptr;
        }

        const TDynArray<TStrongObjectPtr<UIModifier>>& GetModifiers() const noexcept { return m_Modifiers; }

        // =============================================================================
        // Interaction state — what transitions react to (UIModifier.h)
        // =============================================================================
    public:
        /** False when I take no input now (a disabled button): my state is Disabled. */
        virtual bool IsInteractable() const { return true; }

        /** True for a widget whose state the widgets in it can follow (a button: StateSource Button). */
        virtual bool IsStateOwner() const { return false; }

        /** The widget whose state I show: the nearest state owner above me when I follow one, else me. */
        const UIWidget& GetStateSourceWidget() const;

        /** StateSource; my state is worked out again. */
        void SetStateSource(EUIStateSource InSource);

        /**
         * My action fired (a button's click): OnActivated on my enabled modifiers, then on the widgets that
         * follow me. Called by the widget whose action it is, before its own event (a click handler may
         * close the canvas).
         */
        void NotifyActivated();

        //~Begin Object interface
        /** An edited StateSource (or anything my state reads) is worked out again. */
        void PostEditChange() override;
        //~End Object interface

        /**
         * My state now: my preview state when set; my button's when I follow one (StateSource); else
         * from my canvas's pointer and focus: Disabled, else Pressed (held on me or a widget in me, and
         * the pointer still over it), else Hovered, else Focused, else Normal.
         */
        EUIInteractionState GetInteractionState() const;

        /** Works my state out again and tells my modifiers if it changed. Called when IsInteractable changes. */
        void NotifyStateChanged();

        /**
         * Shows InState whatever the pointer and focus do (the editor's Preview state): my art, my
         * transitions, my state tweens. Empty gives me back to the pointer. Runtime only: not saved, and
         * ResetRuntime clears it.
         */
        void SetPreviewState(const TOptional<EUIInteractionState>& InState);
        const TOptional<EUIInteractionState>& GetPreviewState() const noexcept { return m_PreviewState; }

        /** Whether I and every parent have bVisible on. */
        bool IsShownInTree() const noexcept;

        // =============================================================================
        // Show and hide — with the modifiers that animate them. Writing bVisible stays the instant way.
        // =============================================================================
    public:
        /**
         * Shows me (bVisible on) and plays the Show tweens of me and the shown widgets under me. Cancels
         * a hide under way, dropping the pose its tweens held.
         */
        void Show();

        /**
         * Hides me: my Hide tweens play first, and bVisible goes off when the last one ends (at once
         * without one). Not hit while hiding. My children go with me; their own hide tweens do not play.
         */
        void Hide();

        /** Between Hide() and the end of its tweens: still drawn, not hit. */
        bool IsHiding() const noexcept { return m_bHiding; }

        // =============================================================================
        // Clock — a widget that animates itself (a clip) ticks while awake and shown
        // =============================================================================
    public:
        /**
         * Ticked from my canvas's next Tick (from when I join one, if I am in none). While I am not shown
         * I am not ticked; Show() brings me back. Writing bVisible does not: call WakeUp again.
         */
        void WakeUp();

        /** Not ticked any more, from now (Tick answering false does the same at its end). */
        void Sleep();

        /** Whether I asked to be ticked (I may wait for Show while hidden). */
        bool IsAwake() const noexcept { return m_bAwake; }

        /**
         * What I draw now, for automation (ui.list's "brush"): "clip#step", "sheet#frame", a texture path,
         * "none". Empty for a widget that draws no brush.
         */
        virtual OpaaxString DescribeBrush() const { return OpaaxString(); }

    protected:
        /**
         * Runs once per frame while I am awake and shown, with the frame's own time (the game's pause and
         * time scale do not reach the UI).
         * @return False to sleep: not ticked again until WakeUp
         */
        virtual bool Tick(float InSeconds) { (void)InSeconds; return false; }

        /** Back at rest, as if nothing had played (the editor's preview Stop). I am asleep after it. */
        virtual void OnReset() {}

        /** My interaction state changed (GetInteractionState), before my modifiers hear it. */
        virtual void OnInteractionStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo)
        {
            (void)InFrom; (void)InTo;
        }

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

        /** My poses must be composed again (a pose modifier changed); layout and quads are kept. */
        void InvalidatePose();

        // =============================================================================
        // Resolved state (from the last canvas Update)
        // =============================================================================
    public:
        /** My layout rect, in canvas units. Poses do not change it. */
        const Bounds2D&          GetBounds() const noexcept { return m_Bounds; }
        const TDynArray<UIQuad>& GetQuads()  const noexcept { return m_Quads; }

        /**
         * The size I ask of a container that lays me out: Rect.SizeDelta, as my modifiers change it
         * (a size limit clamps it). What UIStackLayout reserves for me.
         */
        Vector2F GetDesiredSize() const;

        /** My own poses composed (my parents' not included). */
        const UIComposedPose&    GetPose()   const noexcept { return m_Pose; }

        /** Where I am drawn: my parents' poses, then mine, in canvas space. Identity when nothing is posed. */
        Affine2D GetDrawTransform() const noexcept;

        /** The opacity I am drawn with: every Opacity and pose opacity from the root down to me. */
        float GetDrawnOpacity() const noexcept;

        /**
         * Where my hit area is: my parents' poses, then mine, keeping only the poses that move the hit
         * area (bMovesHitArea). Identity when none does: I am hit on my layout rect.
         */
        Affine2D GetHitTransform() const noexcept;

        /**
         * InCanvasPoint in my layout space (through the inverse of my hit transform): where pointer
         * events reach me, so a widget compares them with GetBounds() whatever its pose. Far outside
         * every rect when a pose flattens me (a scale of 0).
         */
        Vector2F CanvasToLayout(const Vector2F& InCanvasPoint) const noexcept;

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

        /** Set by a widget whose children show only inside its rect (ClipsChildren). */
        bool m_bClipsChildren = false;

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

        /**
         * The deepest visible, hit-testable widget (or me) containing InPoint, top-most first. Each
         * widget tests the point mapped into its layout space through InParentHit then its own hit pose.
         */
        UIWidget* HitTest(const Vector2F& InPoint, const Affine2D& InParentHit, bool bInParentPosed);

        /** OnPullBindings on me, then every descendant. */
        void PullBindings(UIBindingTable& InBindings);

        void MarkSubtreeUp();
        void SetCanvasRecursive(UICanvas* InCanvas);

        /** InvalidateContent on me and every widget under me. */
        void InvalidateContentRecursive();

        /** m_Pose from my enabled pose modifiers, about pivots in my current bounds. */
        void ComposePose();

        /** OnShow on my modifiers, then on the shown widgets under me. */
        void ShowRecursive();

        /** One of my modifiers finished what OnHide waited for (UIModifier::FinishHide). */
        void OnHideStepDone();

        /** No hide under way, my modifiers and I at rest and asleep, my state worked out again; then my children. */
        void ResetRuntimeRecursive();

        /** NotifyStateChanged on the widgets under me that follow me, down to the next state owner. */
        void NotifyFollowers();

        friend class UIModifier;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UIWidget*                               m_Parent = nullptr;   // not owned
        UICanvas*                               m_Canvas = nullptr;   // not owned; set on attach
        TDynArray<TStrongObjectPtr<UIWidget>>   m_Children;
        TDynArray<TStrongObjectPtr<UIModifier>> m_Modifiers;

        Bounds2D           m_Bounds;
        TDynArray<UIQuad>  m_Quads;
        UIComposedPose     m_Pose;

        bool m_bLayoutDirty  = true;
        bool m_bContentDirty = true;
        bool m_bSubtreeDirty = false;
        bool m_bPoseDirty    = false;
        /** The state my modifiers were last told. */
        EUIInteractionState m_State = EUIInteractionState::Normal;
        /** Between Hide() and its end; how many modifiers it still waits for. */
        bool   m_bHiding     = false;
        Uint32 m_HideWaiting = 0;
        /** Set while I am destroyed: my modifiers' detach must not reach my parents. */
        bool m_bDestroying   = false;
        /** Asked to be ticked (WakeUp), until Tick answers false or Sleep. */
        bool m_bAwake        = false;
        /** The state shown whatever the pointer does (SetPreviewState), or none. */
        TOptional<EUIInteractionState> m_PreviewState;
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIStateSource, Self, Button)
