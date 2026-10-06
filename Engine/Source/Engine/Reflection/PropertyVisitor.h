#pragma once

#include <array>
#include <iterator>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Reflection/TypeInfo.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourceTypeID.hpp"

namespace Opaax
{
    // =============================================================================
    // IPropertyVisitor — walks a reflected object's fields without knowing its type. One call per
    //   field kind; the visitor may read or write the value. This is what lets the editor draw (and a
    //   tool inspect) any registered type with no code written for that type.
    // =============================================================================
    class IPropertyVisitor
    {
    public:
        virtual ~IPropertyVisitor() = default;

        virtual void Visit(const char* InName, bool& InValue, const PropertyMeta& InMeta)          = 0;
        virtual void Visit(const char* InName, Int16& InValue, const PropertyMeta& InMeta)         = 0;
        virtual void Visit(const char* InName, Int32& InValue, const PropertyMeta& InMeta)         = 0;
        virtual void Visit(const char* InName, Uint32& InValue, const PropertyMeta& InMeta)        = 0;
        virtual void Visit(const char* InName, float& InValue, const PropertyMeta& InMeta)         = 0;
        virtual void Visit(const char* InName, Vector2F& InValue, const PropertyMeta& InMeta)      = 0;
        virtual void Visit(const char* InName, Vector3F& InValue, const PropertyMeta& InMeta)      = 0;
        virtual void Visit(const char* InName, Vector4F& InValue, const PropertyMeta& InMeta)      = 0;
        virtual void Visit(const char* InName, LinearColor& InValue, const PropertyMeta& InMeta)   = 0;
        virtual void Visit(const char* InName, OpaaxString& InValue, const PropertyMeta& InMeta)   = 0;
        virtual void Visit(const char* InName, OpaaxStringID& InValue, const PropertyMeta& InMeta) = 0;

        /**
         * An enum, as an index into its labels. Write InOutIndex to change the value.
         * @param InLabels The enumerators' labels, in declaration order
         */
        virtual void VisitEnum(const char* InName, const char* const* InLabels, Uint32 InCount,
                               Uint32& InOutIndex, const PropertyMeta& InMeta) = 0;

        /**
         * A resource reference: its asset-relative path and the resource type it accepts.
         */
        virtual void VisitResourcePath(const char* InName, OpaaxString& InPath, Uint32 InResourceTypeId,
                                       const PropertyMeta& InMeta) = 0;

        /**
         * A data asset reference: its asset-relative path and the data type it accepts ("EnemyStats").
         */
        virtual void VisitDataAssetRef(const char* InName, OpaaxString& InPath, OpaaxStringID InDataType,
                                       const PropertyMeta& InMeta) = 0;

        /**
         * A nested reflected struct. Return false to skip its fields (e.g. a collapsed tree node);
         * EndGroup is only called after a true.
         */
        virtual bool BeginGroup(const char* InName, const PropertyMeta& InMeta) = 0;
        virtual void EndGroup() = 0;

        /** A field of a type no visit exists for. It is reported, not dropped. */
        virtual void VisitUnsupported(const char* InName, std::string_view InTypeName) = 0;
    };

    namespace PropertyVisitorDetail
    {
        template<typename T>
        inline constexpr bool k_IsResourcePath = false;

        template<typename TResource, EResourceLoad TLoad>
        inline constexpr bool k_IsResourcePath<TResourcePath<TResource, TLoad>> = true;

        /** A TDataAssetRef<T>, recognised by its marker (the header is above this one). */
        template<typename T>
        concept CDataAssetRefField = requires(T& InValue)
        {
            typename T::DataType;
            requires T::k_IsDataAssetRef;
            { InValue.Path } -> std::convertible_to<OpaaxString&>;
        };

        template<typename T>
        inline constexpr bool k_IsPlainVisit =
            std::is_same_v<T, bool> || std::is_same_v<T, Int16> || std::is_same_v<T, Int32> ||
            std::is_same_v<T, Uint32> || std::is_same_v<T, float> || std::is_same_v<T, Vector2F> ||
            std::is_same_v<T, Vector3F> || std::is_same_v<T, Vector4F> || std::is_same_v<T, LinearColor> ||
            std::is_same_v<T, OpaaxString> || std::is_same_v<T, OpaaxStringID>;

        /** The labels of E, built once. */
        template<CEnumWithValues E>
        const char* const* EnumLabels() noexcept
        {
            static const auto s_Labels = []
            {
                std::array<const char*, std::size(TEnumValues<E>::Values)> lLabels{};
                for (Uint64 i = 0; i < lLabels.size(); ++i) { lLabels[i] = ToString(TEnumValues<E>::Values[i]); }
                return lLabels;
            }();
            return s_Labels.data();
        }
    }

    template<CReflected TOwner>
    void VisitProperties(TOwner& InOwner, IPropertyVisitor& InVisitor, const PropertyMeta& InInherited = {});

    /**
     * Visits one value. InMeta is already inherited (a group passes its range/step down).
     */
    template<typename TValue>
    void VisitValue(IPropertyVisitor& InVisitor, const char* InName, TValue& InValue, const PropertyMeta& InMeta)
    {
        using namespace PropertyVisitorDetail;

        if constexpr (CReflected<TValue>)
        {
            if (InVisitor.BeginGroup(InName, InMeta))
            {
                VisitProperties(InValue, InVisitor, InMeta);
                InVisitor.EndGroup();
            }
        }
        else if constexpr (k_IsPlainVisit<TValue>)
        {
            InVisitor.Visit(InName, InValue, InMeta);
        }
        else if constexpr (CEnumWithValues<TValue>)
        {
            constexpr Uint32 lCount = static_cast<Uint32>(std::size(TEnumValues<TValue>::Values));

            Uint32 lIndex = 0;
            for (Uint32 i = 0; i < lCount; ++i)
            {
                if (TEnumValues<TValue>::Values[i] == InValue) { lIndex = i; break; }
            }

            const Uint32 lBefore = lIndex;
            InVisitor.VisitEnum(InName, EnumLabels<TValue>(), lCount, lIndex, InMeta);

            if (lIndex != lBefore && lIndex < lCount) { InValue = TEnumValues<TValue>::Values[lIndex]; }
        }
        else if constexpr (k_IsResourcePath<TValue>)
        {
            InVisitor.VisitResourcePath(InName, InValue.Path,
                                        ResourceTypeID::Get<typename TValue::ResourceType>(), InMeta);
        }
        else if constexpr (CDataAssetRefField<TValue>)
        {
            InVisitor.VisitDataAssetRef(InName, InValue.Path, DeriveTypeLeafName<typename TValue::DataType>(), InMeta);
        }
        else
        {
            InVisitor.VisitUnsupported(InName, TypeNameOf<TValue>());
        }
    }

    /**
     * Visits every property InOwner describes, in declaration order.
     */
    template<CReflected TOwner>
    void VisitProperties(TOwner& InOwner, IPropertyVisitor& InVisitor, const PropertyMeta& InInherited)
    {
        std::apply([&InOwner, &InVisitor, &InInherited](const auto&... lProperties)
                   {
                       (VisitValue(InVisitor, lProperties.Name, InOwner.*(lProperties.Member),
                                   InheritMeta(lProperties.Meta, InInherited)), ...);
                   },
                   TOwner::GetProperties());
    }
}
