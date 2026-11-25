#include "Editor/Properties/ObjectPropertyDrawer.h"

#include "Editor/Properties/WidgetPropertyVisitor.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Object/Object.h"

namespace Opaax::Editor
{
    namespace
    {
        // =============================================================================
        // VisibleFieldsVisitor — passes every visit on to InTarget, except the top-level properties
        //   the object hides, each top-level one as the presentation shows it (its label, tooltip,
        //   range). Nested fields and list elements always pass as they are.
        // =============================================================================
        class VisibleFieldsVisitor final : public IPropertyVisitor
        {
        public:
            VisibleFieldsVisitor(IPropertyVisitor& InTarget, const Object& InObject,
                                 const PropertyPresentation* InPresentation)
                : m_Target(InTarget), m_Object(InObject), m_Presentation(InPresentation) {}

            void Visit(const char* InName, bool& InValue, const PropertyMeta& InMeta) override          { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Int16& InValue, const PropertyMeta& InMeta) override         { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Int32& InValue, const PropertyMeta& InMeta) override         { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Uint32& InValue, const PropertyMeta& InMeta) override        { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, float& InValue, const PropertyMeta& InMeta) override         { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Vector2F& InValue, const PropertyMeta& InMeta) override      { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Vector3F& InValue, const PropertyMeta& InMeta) override      { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, Vector4F& InValue, const PropertyMeta& InMeta) override      { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, LinearColor& InValue, const PropertyMeta& InMeta) override   { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, OpaaxString& InValue, const PropertyMeta& InMeta) override   { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }
            void Visit(const char* InName, OpaaxStringID& InValue, const PropertyMeta& InMeta) override { Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown) { m_Target.Visit(InLabel, InValue, InShown); }); }

            void VisitEnum(const char* InName, const char* const* InLabels, const Uint32 InCount, Uint32& InOutIndex,
                           const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitEnum(InLabel, InLabels, InCount, InOutIndex, InShown); });
            }

            void VisitAssetRef(const char* InName, OpaaxString& InPath, const ObjectClass& InClass,
                               const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitAssetRef(InLabel, InPath, InClass, InShown); });
            }

            void VisitObjectRef(const char* InName, ObjectHandle& InHandle, const ObjectClass& InClass,
                                const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitObjectRef(InLabel, InHandle, InClass, InShown); });
            }

            void VisitTag(const char* InName, OpaaxTag& InOutTag, const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitTag(InLabel, InOutTag, InShown); });
            }

            void VisitTagContainer(const char* InName, OpaaxTagContainer& InOutTags, const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitTagContainer(InLabel, InOutTags, InShown); });
            }

            bool BeginGroup(const char* InName, const PropertyMeta& InMeta) override
            {
                bool bOpen = false;
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { bOpen = m_Target.BeginGroup(InLabel, InShown); });
                if (!bOpen)
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
                Pass(InName, PropertyMeta{}, [&](const char* InLabel, const PropertyMeta&)
                     { m_Target.VisitUnsupported(InLabel, InTypeName); });
            }

            void VisitList(const char* InName, IPropertyList& InList, const PropertyMeta& InMeta) override
            {
                Pass(InName, InMeta, [&](const char* InLabel, const PropertyMeta& InShown)
                     { m_Target.VisitList(InLabel, InList, InShown); });
            }

        private:
            /** Forwards a visit as it is shown: nested fields as they are, top-level ones as presented. */
            template<typename TForward>
            void Pass(const char* InName, const PropertyMeta& InMeta, TForward&& InForward)
            {
                if (m_Depth > 0)
                {
                    InForward(InName, InMeta);
                    return;
                }

                if (!m_Object.IsPropertyVisible(OpaaxStringView(InName)))
                {
                    return;
                }

                if (m_Presentation == nullptr || !*m_Presentation)
                {
                    InForward(InName, InMeta);
                    return;
                }

                PropertyPresentationView lView{ OpaaxString(InName), OpaaxString(), InMeta };
                if (!(*m_Presentation)(InName, lView))
                {
                    return;
                }

                if (!lView.Tooltip.IsEmpty())
                {
                    lView.Meta.Tooltip = lView.Tooltip.CStr();
                }
                InForward(lView.Label.CStr(), lView.Meta);
            }

            IPropertyVisitor&           m_Target;
            const Object&               m_Object;
            const PropertyPresentation* m_Presentation = nullptr;
            Uint32                      m_Depth        = 0;
        };
    }

    bool DrawObjectProperties(IEditorWidgets& InWidgets, Object& InObject)
    {
        return DrawObjectProperties(InWidgets, InObject, PropertyPresentation());
    }

    bool DrawObjectProperties(IEditorWidgets& InWidgets, Object& InObject, const PropertyPresentation& InPresentation)
    {
        const ObjectClass& lClass = InObject.GetClass();
        if (lClass.GetPropertyNames().empty())
        {
            return false;
        }

        WidgetPropertyVisitor lWidgets(InWidgets);
        VisibleFieldsVisitor  lVisible(lWidgets, InObject, &InPresentation);
        lClass.VisitProperties(InObject, lVisible);
        return true;
    }
}
