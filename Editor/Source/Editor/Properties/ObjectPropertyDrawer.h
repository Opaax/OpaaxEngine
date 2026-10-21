#pragma once

namespace Opaax
{
    class Object;
}

namespace Opaax::Editor
{
    class IEditorWidgets;

    /**
     * Draws InObject's properties, its parents' first, with the Inspector's widgets. A property the
     * object hides (Object::IsPropertyVisible) is left out. An asset's window and the UI panel use it.
     * @return False when the class has no properties (nothing drawn)
     */
    bool DrawObjectProperties(IEditorWidgets& InWidgets, Object& InObject);
}
