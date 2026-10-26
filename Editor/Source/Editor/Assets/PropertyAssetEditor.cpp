#include "Editor/Assets/PropertyAssetEditor.h"

#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Properties/WidgetPropertyVisitor.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Engine/Reflection/PropertyVisitor.h"

namespace Opaax::Editor
{
    namespace
    {
        // =============================================================================
        // VisibleFieldsVisitor — passes every visit on to InTarget, except the top-level properties
        //   the object hides. Nested fields and list elements always pass.
        // =============================================================================
        class VisibleFieldsVisitor final : public IPropertyVisitor
        {
        public:
            VisibleFieldsVisitor(IPropertyVisitor& InTarget, const Object& InObject)
                : m_Target(InTarget), m_Object(InObject) {}

            void Visit(const char* InName, bool& InValue, const PropertyMeta& InMeta) override          { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Int16& InValue, const PropertyMeta& InMeta) override         { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Int32& InValue, const PropertyMeta& InMeta) override         { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Uint32& InValue, const PropertyMeta& InMeta) override        { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, float& InValue, const PropertyMeta& InMeta) override         { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Vector2F& InValue, const PropertyMeta& InMeta) override      { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Vector3F& InValue, const PropertyMeta& InMeta) override      { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, Vector4F& InValue, const PropertyMeta& InMeta) override      { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, LinearColor& InValue, const PropertyMeta& InMeta) override   { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, OpaaxString& InValue, const PropertyMeta& InMeta) override   { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }
            void Visit(const char* InName, OpaaxStringID& InValue, const PropertyMeta& InMeta) override { if (Shows(InName)) { m_Target.Visit(InName, InValue, InMeta); } }

            void VisitEnum(const char* InName, const char* const* InLabels, const Uint32 InCount, Uint32& InOutIndex,
                           const PropertyMeta& InMeta) override
            {
                if (Shows(InName)) { m_Target.VisitEnum(InName, InLabels, InCount, InOutIndex, InMeta); }
            }

            void VisitResourcePath(const char* InName, OpaaxString& InPath, const Uint32 InResourceTypeId,
                                   const PropertyMeta& InMeta) override
            {
                if (Shows(InName)) { m_Target.VisitResourcePath(InName, InPath, InResourceTypeId, InMeta); }
            }

            void VisitAssetRef(const char* InName, OpaaxString& InPath, const ObjectClass& InClass,
                               const PropertyMeta& InMeta) override
            {
                if (Shows(InName)) { m_Target.VisitAssetRef(InName, InPath, InClass, InMeta); }
            }

            void VisitObjectRef(const char* InName, ObjectHandle& InHandle, const ObjectClass& InClass,
                                const PropertyMeta& InMeta) override
            {
                if (Shows(InName)) { m_Target.VisitObjectRef(InName, InHandle, InClass, InMeta); }
            }

            bool BeginGroup(const char* InName, const PropertyMeta& InMeta) override
            {
                if (!Shows(InName) || !m_Target.BeginGroup(InName, InMeta))
                {
                    return false;
                }

                ++m_Depth;
                return true;
            }

            void EndGroup() override
            {
                --m_Depth;
                m_Target.EndGroup();
            }

            void VisitUnsupported(const char* InName, const OpaaxStringView InTypeName) override
            {
                if (Shows(InName)) { m_Target.VisitUnsupported(InName, InTypeName); }
            }

            void VisitList(const char* InName, IPropertyList& InList, const PropertyMeta& InMeta) override
            {
                if (Shows(InName)) { m_Target.VisitList(InName, InList, InMeta); }
            }

        private:
            bool Shows(const char* InName) const
            {
                return m_Depth > 0 || m_Object.IsPropertyVisible(OpaaxStringView(InName));
            }

            IPropertyVisitor& m_Target;
            const Object&     m_Object;
            Uint32            m_Depth = 0;
        };
    }

    void PropertyAssetEditor::DrawContents(AssetDocument& InDocument)
    {
        DrawProperties(m_Context, InDocument);
    }

    void PropertyAssetEditor::DrawProperties(EditorContext& InContext, AssetDocument& InDocument)
    {
        Asset* lAsset = InDocument.GetAsset();
        if (lAsset == nullptr)
        {
            return;
        }

        const ObjectClass& lClass = lAsset->GetClass();
        if (lClass.GetPropertyNames().empty())
        {
            InContext.Widgets.TextDisabled("This class has no properties.");
            return;
        }

        WidgetPropertyVisitor lWidgets(InContext.Widgets);
        VisibleFieldsVisitor  lVisible(lWidgets, *lAsset);
        lClass.VisitProperties(*lAsset, lVisible);
    }
}
