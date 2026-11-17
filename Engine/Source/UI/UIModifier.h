#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class ObjectClassRegistry;
    class UIWidget;

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

    protected:
        /** I joined GetOwner()'s list. */
        virtual void OnAttach() {}
        /** I am leaving GetOwner()'s list (removed, or the widget is destroyed); GetOwner() is still set. */
        virtual void OnDetach() {}

    private:
        UIWidget* m_Owner = nullptr;   // not owned: it owns me
    };
}
