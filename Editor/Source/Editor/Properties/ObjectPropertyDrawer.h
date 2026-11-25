#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"   // PropertyMeta
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class Object;
}

namespace Opaax::Editor
{
    class IEditorWidgets;

    /** How a property is shown: under which label, with which tooltip, range and step. */
    struct PropertyPresentationView
    {
        OpaaxString  Label;     // the property's name until changed
        OpaaxString  Tooltip;   // when not empty, replaces Meta's tooltip
        PropertyMeta Meta;      // the property's own until changed
    };

    /**
     * Decides how a top-level property of an object is shown: false hides it; otherwise InOutView may
     * be changed (a material shows Param0 under the name its shader gives it).
     */
    using PropertyPresentation = TFunction<bool(const char* InName, PropertyPresentationView& InOutView)>;

    /**
     * Draws InObject's properties, its parents' first, with the Inspector's widgets. A property the
     * object hides (Object::IsPropertyVisible) is left out. An asset's window and the UI panel use it.
     * @return False when the class has no properties (nothing drawn)
     */
    bool DrawObjectProperties(IEditorWidgets& InWidgets, Object& InObject);

    /** The same, each top-level property shown as InPresentation says (after IsPropertyVisible). */
    bool DrawObjectProperties(IEditorWidgets& InWidgets, Object& InObject, const PropertyPresentation& InPresentation);
}
