#include "Editor/Properties/WidgetPropertyVisitor.h"

#include <string>

#include "Editor/Properties/PropertyDrawers.h"

namespace Opaax::Editor
{
    namespace
    {
        template<typename T>
        void DrawWithNote(IEditorWidgets& InWidgets, const char* InName, T& InValue, const PropertyMeta& InMeta)
        {
            TPropertyDrawer<T>::Draw(InWidgets, InName, InValue, InMeta);
            DrawPropertyNote(InWidgets, InMeta);
        }
    }

    void WidgetPropertyVisitor::Visit(const char* InName, bool& InValue, const PropertyMeta& InMeta)          { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Int16& InValue, const PropertyMeta& InMeta)         { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Int32& InValue, const PropertyMeta& InMeta)         { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Uint32& InValue, const PropertyMeta& InMeta)        { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, float& InValue, const PropertyMeta& InMeta)         { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Vector2F& InValue, const PropertyMeta& InMeta)      { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Vector3F& InValue, const PropertyMeta& InMeta)      { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, Vector4F& InValue, const PropertyMeta& InMeta)      { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, LinearColor& InValue, const PropertyMeta& InMeta)   { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, OpaaxString& InValue, const PropertyMeta& InMeta)   { DrawWithNote(m_Widgets, InName, InValue, InMeta); }
    void WidgetPropertyVisitor::Visit(const char* InName, OpaaxStringID& InValue, const PropertyMeta& InMeta) { DrawWithNote(m_Widgets, InName, InValue, InMeta); }

    void WidgetPropertyVisitor::VisitEnum(const char* InName, const char* const* InLabels, const Uint32 InCount,
                                          Uint32& InOutIndex, const PropertyMeta& InMeta)
    {
        DrawEnumIndexField(m_Widgets, InName, InLabels, InCount, InOutIndex);
        DrawPropertyNote(m_Widgets, InMeta);
    }

    void WidgetPropertyVisitor::VisitResourcePath(const char* InName, OpaaxString& InPath,
                                                  const Uint32 InResourceTypeId, const PropertyMeta& InMeta)
    {
        DrawResourcePathField(m_Widgets, InName, InPath, InResourceTypeId);
        DrawPropertyNote(m_Widgets, InMeta);
    }

    void WidgetPropertyVisitor::VisitDataAssetRef(const char* InName, OpaaxString& InPath,
                                                  const OpaaxStringID InDataType, const PropertyMeta& InMeta)
    {
        DrawDataAssetRefField(m_Widgets, InName, InPath, InDataType);
        DrawPropertyNote(m_Widgets, InMeta);
    }

    bool WidgetPropertyVisitor::BeginGroup(const char* InName, const PropertyMeta& InMeta)
    {
        if (!m_Widgets.BeginTreeNode(InName))
        {
            return false;
        }

        DrawPropertyNote(m_Widgets, InMeta);
        return true;
    }

    void WidgetPropertyVisitor::EndGroup()
    {
        m_Widgets.EndTreeNode();
    }

    void WidgetPropertyVisitor::VisitUnsupported(const char* InName, const std::string_view InTypeName)
    {
        // Shown, not hidden: a field that silently vanished is how the old blank-Inspector bug looked.
        const std::string lText = std::string(InName) + ": " + std::string(InTypeName) + " (no editor for this type)";
        m_Widgets.TextDisabled(lText.c_str());
    }
}
