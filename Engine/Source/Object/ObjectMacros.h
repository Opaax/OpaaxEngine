#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Reflection/TypeInfo.h"
#include "Engine/Reflection/PropertyJson.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Object/Object.h"
#include "Object/ObjectClass.h"
#include "Object/ObjectPtr.h"

namespace Opaax::ObjectClassDetail
{
    /** T lists its own properties: OPAAX_PROPERTIES inside T, not one inherited from a parent. */
    template<typename T>
    concept COwnsProperties = CReflected<T> && TIsSame<typename T::PropertyOwnerType, T>;

    /**
     * T lists the files it is read from: OPAAX_ASSET_SOURCE inside T (Resources/Asset/Asset.h), not one
     * inherited from a parent.
     */
    template<typename T>
    concept COwnsSourceExtensions = requires { typename T::SourceOwnerType; }
                                    && TIsSame<typename T::SourceOwnerType, T>;

    /** The description of class T, child of TSuper. Called once, by T::StaticClass(). */
    template<typename T, typename TSuper>
    ObjectClassDesc MakeDesc(const EClassFlags InFlags)
    {
        static_assert(CDerivedFrom<T, TSuper>, "OPAAX_OBJECT(Class, Super): Class must derive from Super.");
        static_assert(CObject<TSuper>, "OPAAX_OBJECT(Class, Super): Super must declare its own OPAAX_OBJECT.");

        ObjectClassDesc lDesc;
        lDesc.Name  = DeriveTypeLeafName<T>();
        lDesc.Id    = TypeIdOf<T>();
        lDesc.Super = &TSuper::StaticClass();
        lDesc.Flags = InFlags;
        lDesc.Size  = sizeof(T);

        if constexpr (!TIsAbstract<T> && CDefaultInitializable<T>)
        {
            if (!HasFlag(InFlags, EClassFlags::Abstract))
            {
                lDesc.Create = []() -> Object* { return new T(); };
            }
        }

        if constexpr (COwnsProperties<T>)
        {
            lDesc.ListOwn = [](TDynArray<const char*>& OutNames)
            {
                TupleApply([&OutNames](const auto&... lProperties) { (OutNames.emplace_back(lProperties.Name), ...); },
                           T::GetProperties());
            };

            lDesc.VisitOwn = [](Object& InObject, IPropertyVisitor& InVisitor)
            {
                VisitProperties(static_cast<T&>(InObject), InVisitor);
            };

            lDesc.WriteOwn = [](const Object& InObject, nlohmann::json& OutJson)
            {
                OutJson.update(PropertiesToJson(static_cast<const T&>(InObject)));
            };

            lDesc.ReadOwn = [](Object& InObject, const nlohmann::json& InJson, TDynArray<OpaaxString>* OutBadFields)
            {
                PropertiesFromJson(InJson, static_cast<T&>(InObject), OutBadFields);
            };
        }

        if constexpr (COwnsSourceExtensions<T>)
        {
            lDesc.SourceExtensions     = T::SourceExtensions;
            lDesc.SourceExtensionCount = static_cast<Uint32>(ArraySize(T::SourceExtensions));
        }

        return lDesc;
    }
}

// =============================================================================
// Declares an Object class: its parent, StaticClass() and GetClass(). Put it first in the class body;
// what follows it is public until another access specifier.
//
//   class MoveMode final : public Opaax::Asset
//   {
//       OPAAX_OBJECT(MoveMode, Opaax::Asset)
//       ...
//   };
// =============================================================================
#define OPAAX_INTERNAL_OBJECT(ClassName, SuperName, InFlags)                                         \
public:                                                                                             \
    using Super     = SuperName;                                                                    \
    using ThisClass = ClassName;                                                                    \
    static const ::Opaax::ObjectClass& StaticClass()                                                \
    {                                                                                               \
        static const ::Opaax::ObjectClass s_Class(                                                  \
            ::Opaax::ObjectClassDetail::MakeDesc<ClassName, SuperName>(InFlags));                   \
        return s_Class;                                                                             \
    }                                                                                               \
    const ::Opaax::ObjectClass& GetClass() const override { return StaticClass(); }

/** An Object class that can be made and saved. */
#define OPAAX_OBJECT(ClassName, SuperName) \
    OPAAX_INTERNAL_OBJECT(ClassName, SuperName, ::Opaax::EClassFlags::None)

/** A base class that is never made by itself (no default object, not offered by editors). */
#define OPAAX_ABSTRACT_OBJECT(ClassName, SuperName) \
    OPAAX_INTERNAL_OBJECT(ClassName, SuperName, ::Opaax::EClassFlags::Abstract)
