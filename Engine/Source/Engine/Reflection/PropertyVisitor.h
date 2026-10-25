#pragma once

#include "Core/Templates/TypeTraits.h"

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Reflection/TypeInfo.h"
#include "Object/ObjectHandle.h"

namespace Opaax
{
    class IPropertyVisitor;
    class ObjectClass;

    // =============================================================================
    // IPropertyList — a list field (TDynArray) seen without its element type: its size, the edits a
    //   visitor may make, and a visit of each element.
    // =============================================================================
    class IPropertyList
    {
    public:
        virtual ~IPropertyList() = default;

        virtual Uint32 Count() const noexcept = 0;

        /** Appends an element with its default value. */
        virtual void Add() = 0;

        virtual void RemoveAt(Uint32 InIndex) = 0;

        /** Swaps two elements (moving one up or down). */
        virtual void Swap(Uint32 InFirst, Uint32 InSecond) = 0;

        /** Visits element InIndex as a field named InName. */
        virtual void VisitElement(Uint32 InIndex, const char* InName, IPropertyVisitor& InVisitor,
                                  const PropertyMeta& InMeta) = 0;
    };

    /** "[3]": the name an element of a list is visited under. */
    struct PropertyListLabel
    {
        char Text[16] = {};

        explicit PropertyListLabel(const Uint32 InIndex) noexcept
        {
            char   lDigits[12] = {};
            Uint32 lCount      = 0;
            Uint32 lValue      = InIndex;
            do
            {
                lDigits[lCount++] = static_cast<char>('0' + lValue % 10);
                lValue /= 10;
            } while (lValue > 0);

            Uint32 lAt = 0;
            Text[lAt++] = '[';
            while (lCount > 0) { Text[lAt++] = lDigits[--lCount]; }
            Text[lAt++] = ']';
        }
    };

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
         * An asset reference (TAssetRef): its asset-relative path and the class it accepts (subclasses too).
         */
        virtual void VisitAssetRef(const char* InName, OpaaxString& InPath, const ObjectClass& InClass,
                                   const PropertyMeta& InMeta) = 0;

        /**
         * An object reference (TObjectPtr): its handle and the class it accepts. Not saved in files.
         */
        virtual void VisitObjectRef(const char* InName, ObjectHandle& InHandle, const ObjectClass& InClass,
                                    const PropertyMeta& InMeta) = 0;

        /**
         * A nested reflected struct. Return false to skip its fields (e.g. a collapsed tree node);
         * EndGroup is only called after a true.
         */
        virtual bool BeginGroup(const char* InName, const PropertyMeta& InMeta) = 0;
        virtual void EndGroup() = 0;

        /** A field of a type no visit exists for. It is reported, not dropped. */
        virtual void VisitUnsupported(const char* InName, OpaaxStringView InTypeName) = 0;

        /** A list. By default each element is visited, named by its index ("[0]"). */
        virtual void VisitList(const char* /*InName*/, IPropertyList& InList, const PropertyMeta& InMeta)
        {
            for (Uint32 lIndex = 0; lIndex < InList.Count(); ++lIndex)
            {
                InList.VisitElement(lIndex, PropertyListLabel(lIndex).Text, *this, InMeta);
            }
        }
    };

    namespace PropertyVisitorDetail
    {
        /** A TAssetRef<T>, recognised by its marker (Resources/Asset/AssetRef.h is not needed here). */
        template<typename T>
        concept CAssetRefField = requires(T& InValue)
        {
            typename T::AssetType;
            requires T::k_IsAssetRef;
            { InValue.Path } -> CConvertibleTo<OpaaxString&>;
        };

        template<typename T>
        inline constexpr bool k_IsList = false;

        template<typename T>
        inline constexpr bool k_IsList<TDynArray<T>> = true;

        template<typename T>
        inline constexpr bool k_IsPlainVisit =
            TIsSame<T, bool> || TIsSame<T, Int16> || TIsSame<T, Int32> ||
            TIsSame<T, Uint32> || TIsSame<T, float> || TIsSame<T, Vector2F> ||
            TIsSame<T, Vector3F> || TIsSame<T, Vector4F> || TIsSame<T, LinearColor> ||
            TIsSame<T, OpaaxString> || TIsSame<T, OpaaxStringID>;

        /** The labels of E, built once. */
        template<CEnumWithValues E>
        const char* const* EnumLabels() noexcept
        {
            static const auto s_Labels = []
            {
                TFixedArray<const char*, ArraySize(TEnumValues<E>::Values)> lLabels{};
                for (Uint64 i = 0; i < lLabels.size(); ++i) { lLabels[i] = ToString(TEnumValues<E>::Values[i]); }
                return lLabels;
            }();
            return s_Labels.data();
        }
    }

    template<CReflected TOwner>
    void VisitProperties(TOwner& InOwner, IPropertyVisitor& InVisitor, const PropertyMeta& InInherited = {});

    template<typename TValue>
    void VisitValue(IPropertyVisitor& InVisitor, const char* InName, TValue& InValue, const PropertyMeta& InMeta);

    // =============================================================================
    // TPropertyList — a TDynArray<T> as an IPropertyList.
    // =============================================================================
    template<typename TElement>
    class TPropertyList final : public IPropertyList
    {
    public:
        explicit TPropertyList(TDynArray<TElement>& InValues) : m_Values(InValues) {}

        Uint32 Count() const noexcept override { return static_cast<Uint32>(m_Values.size()); }

        void Add() override { m_Values.emplace_back(); }

        void RemoveAt(const Uint32 InIndex) override
        {
            if (InIndex < m_Values.size())
            {
                m_Values.erase(m_Values.begin() + InIndex);
            }
        }

        void Swap(const Uint32 InFirst, const Uint32 InSecond) override
        {
            if (InFirst < m_Values.size() && InSecond < m_Values.size())
            {
                Opaax::Swap(m_Values[InFirst], m_Values[InSecond]);
            }
        }

        void VisitElement(const Uint32 InIndex, const char* InName, IPropertyVisitor& InVisitor,
                          const PropertyMeta& InMeta) override
        {
            if (InIndex < m_Values.size())
            {
                VisitValue(InVisitor, InName, m_Values[InIndex], InMeta);
            }
        }

    private:
        TDynArray<TElement>& m_Values;
    };

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
            constexpr Uint32 lCount = static_cast<Uint32>(ArraySize(TEnumValues<TValue>::Values));

            Uint32 lIndex = 0;
            for (Uint32 i = 0; i < lCount; ++i)
            {
                if (TEnumValues<TValue>::Values[i] == InValue) { lIndex = i; break; }
            }

            const Uint32 lBefore = lIndex;
            InVisitor.VisitEnum(InName, EnumLabels<TValue>(), lCount, lIndex, InMeta);

            if (lIndex != lBefore && lIndex < lCount) { InValue = TEnumValues<TValue>::Values[lIndex]; }
        }
        else if constexpr (CAssetRefField<TValue>)
        {
            InVisitor.VisitAssetRef(InName, InValue.Path, TValue::AssetType::StaticClass(), InMeta);
        }
        else if constexpr (CObjectPtrField<TValue>)
        {
            InVisitor.VisitObjectRef(InName, InValue.GetMutableHandle(), TValue::ObjectType::StaticClass(), InMeta);
        }
        else if constexpr (k_IsList<TValue> && !TIsSame<TValue, TDynArray<bool>>)
        {
            TPropertyList<typename TValue::value_type> lList(InValue);
            InVisitor.VisitList(InName, lList, InMeta);
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
        TupleApply([&InOwner, &InVisitor, &InInherited](const auto&... lProperties)
                   {
                       (VisitValue(InVisitor, lProperties.Name, InOwner.*(lProperties.Member),
                                   InheritMeta(lProperties.Meta, InInherited)), ...);
                   },
                   TOwner::GetProperties());
    }
}
