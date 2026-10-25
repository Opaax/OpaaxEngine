#include "Object/ObjectClass.h"

#include "Core/String/CString.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Object/Object.h"
#include "Object/ObjectPtr.h"
#include "Object/ObjectTable.h"

namespace Opaax
{
    namespace
    {
        /** Every class from the root down to InClass: the order properties are visited and saved in. */
        TDynArray<const ObjectClass*> ChainOf(const ObjectClass& InClass)
        {
            TDynArray<const ObjectClass*> lChain;

            for (const ObjectClass* lClass = &InClass; lClass != nullptr; lClass = lClass->GetSuper())
            {
                lChain.insert(lChain.begin(), lClass);
            }

            return lChain;
        }

        /** The handle of every object reference, every group and list opened. */
        class ReferenceCollector final : public IPropertyVisitor
        {
        public:
            TDynArray<ObjectHandle> Handles;

            void Visit(const char*, bool&, const PropertyMeta&) override          {}
            void Visit(const char*, Int16&, const PropertyMeta&) override         {}
            void Visit(const char*, Int32&, const PropertyMeta&) override         {}
            void Visit(const char*, Uint32&, const PropertyMeta&) override        {}
            void Visit(const char*, float&, const PropertyMeta&) override         {}
            void Visit(const char*, Vector2F&, const PropertyMeta&) override      {}
            void Visit(const char*, Vector3F&, const PropertyMeta&) override      {}
            void Visit(const char*, Vector4F&, const PropertyMeta&) override      {}
            void Visit(const char*, LinearColor&, const PropertyMeta&) override   {}
            void Visit(const char*, OpaaxString&, const PropertyMeta&) override   {}
            void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
            void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}

            void VisitAssetRef(const char*, OpaaxString&, const ObjectClass&, const PropertyMeta&) override {}

            void VisitObjectRef(const char*, ObjectHandle& InHandle, const ObjectClass&, const PropertyMeta&) override
            {
                if (InHandle.IsValid())
                {
                    Handles.emplace_back(InHandle);
                }
            }

            bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
            void EndGroup() override {}
            void VisitUnsupported(const char*, OpaaxStringView) override {}
        };
    }

    ObjectClass::ObjectClass(const ObjectClassDesc& InDesc)
        : m_Desc(InDesc)
    {
        if (m_Desc.ListOwn == nullptr || m_Desc.Super == nullptr)
        {
            return;
        }

        // A property named like a parent's would be written once and read into both fields.
        TDynArray<const char*> lOwn;
        m_Desc.ListOwn(lOwn);

        const TDynArray<const char*> lInherited = m_Desc.Super->GetPropertyNames();

        for (const char* lName : lOwn)
        {
            for (const char* lParentName : lInherited)
            {
                if (CString::Equals(lName, lParentName))
                {
                    OPAAX_LOG(LogObject, Error, "Class '{}': property '{}' repeats a parent's: rename it",
                              m_Desc.Name.CStr(), lName);
                }
            }
        }
    }

    bool ObjectClass::IsChildOf(const ObjectClass& InOther) const noexcept
    {
        for (const ObjectClass* lClass = this; lClass != nullptr; lClass = lClass->GetSuper())
        {
            if (lClass->GetTypeId() == InOther.GetTypeId())
            {
                return true;
            }
        }

        return false;
    }

    TStrongObjectPtr<Object> ObjectClass::CreateInstance(const OpaaxStringID InName) const
    {
        if (m_Desc.Create == nullptr)
        {
            OPAAX_LOG(LogObject, Error, "Class '{}' is abstract: no instance made", m_Desc.Name.CStr());
            return {};
        }

        Object* lObject = m_Desc.Create();

        const ObjectHandle lHandle = lObject->GetHandle();
        ObjectTable::Get().AddFlags(lHandle, EObjectFlags::Managed);
        lObject->SetName(InName.IsValid() ? InName : ObjectPtrDetail::MakeDefaultName(*this, lHandle));

        return TStrongObjectPtr<Object>(lObject);
    }

    const Object* ObjectClass::GetDefaultObject() const
    {
        if (m_Desc.Create == nullptr)
        {
            return nullptr;
        }

        TLockGuard<Mutex> lLock(m_DefaultLock);

        if (m_Default == nullptr)
        {
            // Kept for the whole run, like the class itself.
            m_Default = m_Desc.Create();
            ObjectTable::Get().AddFlags(m_Default->GetHandle(), EObjectFlags::ClassDefault);
            m_Default->SetName(OpaaxStringID(OpaaxString::Format("Default__{}", m_Desc.Name.CStr())));
        }

        return m_Default;
    }

    TDynArray<const char*> ObjectClass::GetSourceExtensions() const
    {
        TDynArray<const char*> lExtensions;
        lExtensions.reserve(m_Desc.SourceExtensionCount);

        for (Uint32 i = 0; i < m_Desc.SourceExtensionCount; ++i)
        {
            lExtensions.emplace_back(m_Desc.SourceExtensions[i]);
        }

        return lExtensions;
    }

    TDynArray<const char*> ObjectClass::GetPropertyNames() const
    {
        TDynArray<const char*> lNames;

        for (const ObjectClass* lClass : ChainOf(*this))
        {
            if (lClass->m_Desc.ListOwn != nullptr)
            {
                lClass->m_Desc.ListOwn(lNames);
            }
        }

        return lNames;
    }

    void ObjectClass::VisitProperties(Object& InObject, IPropertyVisitor& InVisitor) const
    {
        if (!InObject.IsA(*this))
        {
            OPAAX_LOG(LogObject, Error, "'{}' is not a '{}': its properties are not visited",
                      InObject.GetName().CStr(), m_Desc.Name.CStr());
            return;
        }

        for (const ObjectClass* lClass : ChainOf(*this))
        {
            if (lClass->m_Desc.VisitOwn != nullptr)
            {
                lClass->m_Desc.VisitOwn(InObject, InVisitor);
            }
        }
    }

    nlohmann::json ObjectClass::WriteProperties(const Object& InObject) const
    {
        nlohmann::json lJson = nlohmann::json::object();

        if (!InObject.IsA(*this))
        {
            OPAAX_LOG(LogObject, Error, "'{}' is not a '{}': no properties written",
                      InObject.GetName().CStr(), m_Desc.Name.CStr());
            return lJson;
        }

        for (const ObjectClass* lClass : ChainOf(*this))
        {
            if (lClass->m_Desc.WriteOwn != nullptr)
            {
                lClass->m_Desc.WriteOwn(InObject, lJson);
            }
        }

        return lJson;
    }

    bool ObjectClass::ReadProperties(Object& InObject, const nlohmann::json& InJson,
                                     TDynArray<OpaaxString>* OutBadFields) const
    {
        if (!InJson.is_object())
        {
            return false;
        }

        if (!InObject.IsA(*this))
        {
            OPAAX_LOG(LogObject, Error, "'{}' is not a '{}': no properties read",
                      InObject.GetName().CStr(), m_Desc.Name.CStr());
            return false;
        }

        for (const ObjectClass* lClass : ChainOf(*this))
        {
            if (lClass->m_Desc.ReadOwn != nullptr)
            {
                lClass->m_Desc.ReadOwn(InObject, InJson, OutBadFields);
            }
        }

        return true;
    }

    TDynArray<ObjectHandle> ObjectClass::CollectReferences(Object& InObject) const
    {
        ReferenceCollector lCollector;
        VisitProperties(InObject, lCollector);
        return Move(lCollector.Handles);
    }
}
