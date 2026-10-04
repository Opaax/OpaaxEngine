#pragma once

#include "Engine/Reflection/PropertyVisitor.h"

namespace Opaax::Editor
{
    class IEditorWidgets;

    // =============================================================================
    // WidgetPropertyVisitor — draws a type-erased object with the normal property drawers. Each visit
    //   calls the same TPropertyDrawer<T> as DrawProperties, so a field looks the same whichever path
    //   drew it.
    // =============================================================================
    class WidgetPropertyVisitor final : public IPropertyVisitor
    {
    public:
        explicit WidgetPropertyVisitor(IEditorWidgets& InWidgets) : m_Widgets(InWidgets) {}

        void Visit(const char* InName, bool& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Int16& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Int32& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Uint32& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, float& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Vector2F& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Vector3F& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, Vector4F& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, LinearColor& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, OpaaxString& InValue, const PropertyMeta& InMeta) override;
        void Visit(const char* InName, OpaaxStringID& InValue, const PropertyMeta& InMeta) override;

        void VisitEnum(const char* InName, const char* const* InLabels, Uint32 InCount,
                       Uint32& InOutIndex, const PropertyMeta& InMeta) override;

        void VisitResourcePath(const char* InName, OpaaxString& InPath, Uint32 InResourceTypeId,
                               const PropertyMeta& InMeta) override;

        bool BeginGroup(const char* InName, const PropertyMeta& InMeta) override;
        void EndGroup() override;

        void VisitUnsupported(const char* InName, std::string_view InTypeName) override;

    private:
        IEditorWidgets& m_Widgets;
    };
}
