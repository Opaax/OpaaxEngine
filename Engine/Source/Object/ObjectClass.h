#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/TypeInfo.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectHandle.h"

namespace Opaax
{
    class IPropertyVisitor;
    class Object;
    class ObjectClass;

    template<typename T>
    class TStrongObjectPtr;

    /** What a class allows (bitmask). */
    enum class EClassFlags : Uint8
    {
        None     = 0,

        /** Never instantiated: a base for other classes (Asset). */
        Abstract = BIT(0),
    };

    constexpr bool HasFlag(const EClassFlags InValue, const EClassFlags InFlag) noexcept
    {
        return (static_cast<Uint8>(InValue) & static_cast<Uint8>(InFlag)) != 0;
    }

    // =============================================================================
    // ObjectClassDesc — what one class is made of, filled by OPAAX_OBJECT from the C++ type. The
    //   property functions cover the class's own OPAAX_PROPERTIES only: ObjectClass chains the parents.
    // =============================================================================
    struct ObjectClassDesc
    {
        OpaaxStringID      Name;
        TypeId             Id    = 0;
        const ObjectClass* Super = nullptr;
        EClassFlags        Flags = EClassFlags::None;
        Uint64             Size  = 0;

        /** A new instance on the heap. Null for an abstract class. */
        Object* (*Create)() = nullptr;

        /** The names of the class's own properties. Null when it lists none. */
        void (*ListOwn)(TDynArray<const char*>& OutNames) = nullptr;

        void (*VisitOwn)(Object& InObject, IPropertyVisitor& InVisitor) = nullptr;
        void (*WriteOwn)(const Object& InObject, nlohmann::json& OutJson) = nullptr;
        void (*ReadOwn)(Object& InObject, const nlohmann::json& InJson, TDynArray<OpaaxString>* OutBadFields) = nullptr;
    };

    // =============================================================================
    // ObjectClass — an Object type at run time (Unreal's UClass): its saved name, its parent, how to
    //   make one, and its properties, read and written without knowing the C++ type. One per class,
    //   built by the class's StaticClass() the first time it is asked.
    // =============================================================================
    class ObjectClass
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        /** Logs an error when a property repeats one of a parent class (it would be saved once). */
        explicit ObjectClass(const ObjectClassDesc& InDesc);

        /** The default object is never destroyed (handles to it stay valid until exit). */
        ~ObjectClass() = default;

        ObjectClass(const ObjectClass&)            = delete;
        ObjectClass& operator=(const ObjectClass&) = delete;

        // =============================================================================
        // Identity
        // =============================================================================
    public:
        /** The name written in files ("MoveMode"): the C++ name without namespace. */
        OpaaxStringID GetName() const noexcept { return m_Desc.Name; }

        TypeId GetTypeId() const noexcept { return m_Desc.Id; }

        /** The parent class. Null only for Object. */
        const ObjectClass* GetSuper() const noexcept { return m_Desc.Super; }

        /** sizeof the C++ type. */
        Uint64 GetSize() const noexcept { return m_Desc.Size; }

        bool IsAbstract() const noexcept { return m_Desc.Create == nullptr; }

        /** True when this class is InOther or derives from it. */
        bool IsChildOf(const ObjectClass& InOther) const noexcept;

        // =============================================================================
        // Instances
        // =============================================================================
    public:
        /**
         * A new object of this class, owned by the returned pointer (NewObject without the C++ type).
         * @param InName Its name. Invalid gives "<Class>_<slot>"
         * @return Empty for an abstract class
         */
        TStrongObjectPtr<Object> CreateInstance(OpaaxStringID InName = {}) const;

        /**
         * The class's default object: every field at its default. Made the first time it is asked, and
         * never destroyed.
         * @return Null for an abstract class
         */
        const Object* GetDefaultObject() const;

        // =============================================================================
        // Properties, parents first
        // =============================================================================
    public:
        /** The names of every property, parents' first. */
        TDynArray<const char*> GetPropertyNames() const;

        /** Visits every property of InObject (an instance of this class), parents' first. */
        void VisitProperties(Object& InObject, IPropertyVisitor& InVisitor) const;

        /** Every property of InObject as one JSON object, keyed by property name. */
        nlohmann::json WriteProperties(const Object& InObject) const;

        /**
         * Reads InJson into InObject's properties. A missing key keeps the field's value; a value of the
         * wrong type keeps it too, and its name is added to OutBadFields.
         * @return False if InJson is not an object (nothing is read)
         */
        bool ReadProperties(Object& InObject, const nlohmann::json& InJson,
                            TDynArray<OpaaxString>* OutBadFields = nullptr) const;

        /**
         * The object references (TObjectPtr fields) of InObject, nested structs and lists included: what a
         * garbage collector follows.
         */
        TDynArray<ObjectHandle> CollectReferences(Object& InObject) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ObjectClassDesc m_Desc;

        mutable Mutex   m_DefaultLock;
        mutable Object* m_Default = nullptr;
    };
}
