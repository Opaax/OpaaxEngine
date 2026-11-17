#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class ObjectClassRegistry;
    class UICanvas;
    class UIWidget;

    /**
     * How the pointer and the game see a widget, the strongest first: Disabled (it does not take input),
     * Pressed (held on it), Hovered (under the pointer), Focused (takes the keys), Normal. Hovered,
     * pressed and focused reach the parents: a panel is hovered while a widget in it is.
     */
    enum class EUIInteractionState : Uint8
    {
        Normal,
        Hovered,
        Pressed,
        Focused,
        Disabled
    };

    inline const char* ToString(const EUIInteractionState InState) noexcept
    {
        switch (InState)
        {
        case EUIInteractionState::Normal:   return "Normal";
        case EUIInteractionState::Hovered:  return "Hovered";
        case EUIInteractionState::Pressed:  return "Pressed";
        case EUIInteractionState::Focused:  return "Focused";
        case EUIInteractionState::Disabled: return "Disabled";
        }
        return "Normal";
    }
}

OPAAX_ENUM_VALUES(Opaax::EUIInteractionState, Normal, Hovered, Pressed, Focused, Disabled)

namespace Opaax
{
    // =============================================================================
    // UIModifier — a small object a widget owns, with one job (a transition, a shadow, a size limit).
    //   An Object like a widget: its class is the name a .opaaxui saves in the node's "Modifiers", its
    //   properties are what the file saves and the UI panel draws. A widget keeps its modifiers in the
    //   order they were added; a new type is a subclass with OPAAX_OBJECT, OPAAX_PROPERTIES and one
    //   OPAAX_REGISTER_OBJECT line. Code that keeps one it does not own holds a TWeakObjectPtr.
    // =============================================================================
    class UIModifier : public Object
    {
        OPAAX_ABSTRACT_OBJECT(UIModifier, Object)

        friend class UICanvas;
        friend class UIWidget;

    public:
        /** Found by code and scripts (FindModifier<T>(OPAAX_ID("Intro"))); may be empty. */
        OpaaxStringID Name;
        /** Off: the modifier does nothing (it stays in the file). */
        bool          bEnabled = true;

        OPAAX_PROPERTIES(UIModifier,
                         OPAAX_PROP(Name).SetTooltip("Found by code and scripts by this name. May be empty."),
                         OPAAX_PROP(bEnabled).SetTooltip("Off: the modifier does nothing, and stays in the file."))

        /**
         * A new modifier of the class named InClassName (what a file or the editor's Add menu names).
         * @return Empty when InClasses has no such class, or it is abstract or not a modifier
         */
        static TStrongObjectPtr<UIModifier> Create(OpaaxStringID InClassName, const ObjectClassRegistry& InClasses);

        /** The widget that owns me, or null before I am added and after I am removed. */
        UIWidget* GetOwner() const noexcept { return m_Owner; }

        /** Whether my owner's canvas ticks me (until Tick answers false). */
        bool IsAwake() const noexcept { return m_bAwake; }

    protected:
        /** I joined GetOwner()'s list. */
        virtual void OnAttach() {}
        /** I am leaving GetOwner()'s list (removed, or the widget is destroyed); GetOwner() is still set. */
        virtual void OnDetach() {}

        /**
         * My owner was shown: it joined a canvas the game ticks, shown (an editor's document is not
         * ticked: nothing plays there until its preview does).
         */
        virtual void OnShow() {}

        /**
         * My owner is being hidden with Hide().
         * @return True while hiding takes time (a fade): the widget hides once every such modifier
         *   called FinishHide
         */
        virtual bool OnHide() { return false; }

        /**
         * Back at rest, as if nothing had played (the editor's preview Stop): no motion, no hide to
         * finish. My canvas forgets I was awake.
         */
        virtual void OnReset() {}

        /** My owner's interaction state changed (UIWidget::GetInteractionState). */
        virtual void OnStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo) { (void)InFrom; (void)InTo; }

        /**
         * Runs once per frame while I am awake, with the frame's own time (the game's pause and time
         * scale do not reach the UI).
         * @return False to sleep: not ticked again until WakeUp
         */
        virtual bool Tick(float InSeconds) { (void)InSeconds; return false; }

        /** Ticked from the next Tick of my owner's canvas (from when I join one, if I am in none). */
        void WakeUp();

        /** Not ticked any more, from now (Tick answering false does the same at its end). */
        void Sleep();

        /** What OnHide waited for is done: once every such modifier says so, the owner hides. */
        void FinishHide();

    private:
        UIWidget* m_Owner  = nullptr;   // not owned: it owns me
        bool      m_bAwake = false;
    };
}
