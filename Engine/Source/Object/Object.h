#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Object/ObjectClass.h"
#include "Object/ObjectHandle.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// Object is the root of the engine's object types (Unreal's UObject). A class names its parent
// with OPAAX_OBJECT, lists its fields with OPAAX_PROPERTIES and registers with one line:
//
//   class EnemyStats final : public Opaax::Asset
//   {
//       OPAAX_OBJECT(EnemyStats, Opaax::Asset)
//
//   public:
//       float Health = 100.f;
//       OPAAX_PROPERTIES(EnemyStats, OPAAX_PROP(Health).SetRange(1.f, 1000.f))
//   };
//   OPAAX_REGISTER_OBJECT(EnemyStats);
//
// What it gives: GetClass() and IsA/Cast, a name and a handle, its properties read, written and
// edited without its C++ type, a default object, and a slot in the ObjectTable so handles to it
// (TWeakObjectPtr, TObjectPtr) read null once it is gone. NewObject<T>() makes one owned by a
// TStrongObjectPtr; the last owner destroys it.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Object);

    // =============================================================================
    // Object — the root class. Not copyable: an object is an identity (its slot), not a value.
    // =============================================================================
    class Object
    {
        // =============================================================================
        // Class
        // =============================================================================
    public:
        using ThisClass = Object;

        static const ObjectClass& StaticClass();

        /** The class of the object's dynamic type. */
        virtual const ObjectClass& GetClass() const { return StaticClass(); }

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        /** Takes a slot in the ObjectTable. */
        Object();

        /** Gives its slot back: handles to it read null from here on. */
        virtual ~Object();

        Object(const Object&)            = delete;
        Object& operator=(const Object&) = delete;
        Object(Object&&)                 = delete;
        Object& operator=(Object&&)      = delete;

        // =============================================================================
        // Identity
        // =============================================================================
    public:
        ObjectHandle GetHandle() const noexcept { return m_Handle; }

        /** For logs and tools. An asset is named after its file. */
        OpaaxStringID GetName() const noexcept { return m_Name; }
        void          SetName(const OpaaxStringID InName) noexcept { m_Name = InName; }

        EObjectFlags GetFlags() const;

        /** True if the object has any of InFlags. */
        bool HasAnyFlags(EObjectFlags InFlags) const;

        /** True when the object's class is InClass or derives from it. */
        bool IsA(const ObjectClass& InClass) const noexcept { return GetClass().IsChildOf(InClass); }

        template<typename T>
        bool IsA() const noexcept { return IsA(T::StaticClass()); }

        // =============================================================================
        // Hooks
        // =============================================================================
    public:
        /**
         * The properties were just read (a file loaded, an undo applied). Derive what the saved fields
         * imply here. May run on a loading thread: touch nothing but the object.
         */
        virtual void PostLoad() {}

        /** An editor just changed one or more properties. Fix up or validate the values here. */
        virtual void PostEditChange() {}

        /**
         * Whether an editor shows the property named InName. Lets a class hide the fields that do not
         * apply to its current settings.
         */
        virtual bool IsPropertyVisible(OpaaxStringView /*InName*/) const { return true; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ObjectHandle  m_Handle;
        OpaaxStringID m_Name;
    };

    /** A type that is an Object and declares its own class (OPAAX_OBJECT), so GetClass() names it. */
    template<typename T>
    concept CObject = CDerivedFrom<T, Object> && TIsSame<typename T::ThisClass, T>;

    /** InObject as a T when it is one, else null. */
    template<typename T>
    T* Cast(Object* InObject) noexcept
    {
        return (InObject != nullptr && InObject->IsA<T>()) ? static_cast<T*>(InObject) : nullptr;
    }

    template<typename T>
    const T* Cast(const Object* InObject) noexcept
    {
        return (InObject != nullptr && InObject->IsA<T>()) ? static_cast<const T*>(InObject) : nullptr;
    }
}
